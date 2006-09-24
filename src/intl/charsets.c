/* Charsets convertor */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#if HAVE_WCTYPE_H
#include <wctype.h>
#endif

#include "elinks.h"

#include "document/options.h"
#include "intl/charsets.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/fastfind.h"
#include "util/memory.h"
#include "util/string.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

struct table_entry {
	unsigned char c;
	/* This should in principle be unicode_val_T, but because all
	 * the values currently in codepage.inc fit in 16 bits, we can
	 * as well use uint16_t and halve sizeof(struct table_entry)
	 * from 8 bytes to 4.  Should other characters ever be needed,
	 * unicode_val_T u : 24 might be a possibility, although it
	 * seems a little unportable as bitfields are in principle
	 * restricted to int, which may be 16-bit.  */
	uint16_t u;
};

struct codepage_desc {
	unsigned char *name;
	unsigned char *const *aliases;
	const struct table_entry *table;
};

#include "intl/codepage.inc"
#include "intl/uni_7b.inc"
#include "intl/entity.inc"


static char strings[256][2] = {
	"\000", "\001", "\002", "\003", "\004", "\005", "\006", "\007",
	"\010", "\011", "\012", "\013", "\014", "\015", "\016", "\017",
	"\020", "\021", "\022", "\023", "\024", "\025", "\026", "\033",
	"\030", "\031", "\032", "\033", "\034", "\035", "\036", "\033",
	"\040", "\041", "\042", "\043", "\044", "\045", "\046", "\047",
	"\050", "\051", "\052", "\053", "\054", "\055", "\056", "\057",
	"\060", "\061", "\062", "\063", "\064", "\065", "\066", "\067",
	"\070", "\071", "\072", "\073", "\074", "\075", "\076", "\077",
	"\100", "\101", "\102", "\103", "\104", "\105", "\106", "\107",
	"\110", "\111", "\112", "\113", "\114", "\115", "\116", "\117",
	"\120", "\121", "\122", "\123", "\124", "\125", "\126", "\127",
	"\130", "\131", "\132", "\133", "\134", "\135", "\136", "\137",
	"\140", "\141", "\142", "\143", "\144", "\145", "\146", "\147",
	"\150", "\151", "\152", "\153", "\154", "\155", "\156", "\157",
	"\160", "\161", "\162", "\163", "\164", "\165", "\166", "\167",
	"\170", "\171", "\172", "\173", "\174", "\175", "\176", "\177",
	"\200", "\201", "\202", "\203", "\204", "\205", "\206", "\207",
	"\210", "\211", "\212", "\213", "\214", "\215", "\216", "\217",
	"\220", "\221", "\222", "\223", "\224", "\225", "\226", "\227",
	"\230", "\231", "\232", "\233", "\234", "\235", "\236", "\237",
	"\240", "\241", "\242", "\243", "\244", "\245", "\246", "\247",
	"\250", "\251", "\252", "\253", "\254", "\255", "\256", "\257",
	"\260", "\261", "\262", "\263", "\264", "\265", "\266", "\267",
	"\270", "\271", "\272", "\273", "\274", "\275", "\276", "\277",
	"\300", "\301", "\302", "\303", "\304", "\305", "\306", "\307",
	"\310", "\311", "\312", "\313", "\314", "\315", "\316", "\317",
	"\320", "\321", "\322", "\323", "\324", "\325", "\326", "\327",
	"\330", "\331", "\332", "\333", "\334", "\335", "\336", "\337",
	"\340", "\341", "\342", "\343", "\344", "\345", "\346", "\347",
	"\350", "\351", "\352", "\353", "\354", "\355", "\356", "\357",
	"\360", "\361", "\362", "\363", "\364", "\365", "\366", "\367",
	"\370", "\371", "\372", "\373", "\374", "\375", "\376", "\377",
};

static void
free_translation_table(struct conv_table *p)
{
	int i;

	for (i = 0; i < 256; i++)
		if (p[i].t)
			free_translation_table(p[i].u.tbl);

	mem_free(p);
}

static unsigned char *no_str = "*";

static void
new_translation_table(struct conv_table *p)
{
	int i;

	for (i = 0; i < 256; i++)
		if (p[i].t)
			free_translation_table(p[i].u.tbl);
	for (i = 0; i < 128; i++) {
		p[i].t = 0;
		p[i].u.str = strings[i];
	}
	for (; i < 256; i++) {
		p[i].t = 0;
	       	p[i].u.str = no_str;
	}
}

#define BIN_SEARCH(table, entry, entries, key, result)					\
{											\
	long _s = 0, _e = (entries) - 1;						\
											\
	while (_s <= _e || !((result) = -1)) {						\
		long _m = (_s + _e) / 2;						\
											\
		if ((table)[_m].entry == (key)) {					\
			(result) = _m;							\
			break;								\
		}									\
		if ((table)[_m].entry > (key)) _e = _m - 1;				\
		if ((table)[_m].entry < (key)) _s = _m + 1;				\
	}										\
}											\

static const unicode_val_T strange_chars[32] = {
0x20ac, 0x0000, 0x002a, 0x0000, 0x201e, 0x2026, 0x2020, 0x2021,
0x005e, 0x2030, 0x0160, 0x003c, 0x0152, 0x0000, 0x0000, 0x0000,
0x0000, 0x0060, 0x0027, 0x0022, 0x0022, 0x002a, 0x2013, 0x2014,
0x007e, 0x2122, 0x0161, 0x003e, 0x0153, 0x0000, 0x0000, 0x0000,
};

#define SYSTEM_CHARSET_FLAG 128

unsigned char *
u2cp_(unicode_val_T u, int to, int no_nbsp_hack)
{
	int j;
	int s;

	if (u < 128) return strings[u];

	to &= ~SYSTEM_CHARSET_FLAG;

#ifdef CONFIG_UTF8
	if (codepages[to].table == table_utf8)
		return encode_utf8(u);
#endif /* CONFIG_UTF8 */

	/* To mark non breaking spaces, we use a special char NBSP_CHAR. */
	if (u == 0xa0) return no_nbsp_hack ? " " : NBSP_CHAR_STRING;
	if (u == 0xad) return "";

	if (u < 0xa0) {
		unicode_val_T strange = strange_chars[u - 0x80];

		if (!strange) return NULL;
		return u2cp_(strange, to, no_nbsp_hack);
	}


	for (j = 0; codepages[to].table[j].c; j++)
		if (codepages[to].table[j].u == u)
			return strings[codepages[to].table[j].c];

	BIN_SEARCH(unicode_7b, x, N_UNICODE_7B, u, s);
	if (s != -1) return unicode_7b[s].s;

	return no_str;
}

static unsigned char utf_buffer[7];

#ifdef CONFIG_UTF8
inline unsigned char *
encode_utf8(unicode_val_T u)
#else
static unsigned char *
encode_utf8(unicode_val_T u)
#endif /* CONFIG_UTF8 */
{
	memset(utf_buffer, 0, 7);

	if (u < 0x80)
		utf_buffer[0] = u;
	else if (u < 0x800)
		utf_buffer[0] = 0xc0 | ((u >> 6) & 0x1f),
		utf_buffer[1] = 0x80 | (u & 0x3f);
	else if (u < 0x10000)
		utf_buffer[0] = 0xe0 | ((u >> 12) & 0x0f),
		utf_buffer[1] = 0x80 | ((u >> 6) & 0x3f),
		utf_buffer[2] = 0x80 | (u & 0x3f);
	else if (u < 0x200000)
		utf_buffer[0] = 0xf0 | ((u >> 18) & 0x0f),
		utf_buffer[1] = 0x80 | ((u >> 12) & 0x3f),
		utf_buffer[2] = 0x80 | ((u >> 6) & 0x3f),
		utf_buffer[3] = 0x80 | (u & 0x3f);
	else if (u < 0x4000000)
		utf_buffer[0] = 0xf8 | ((u >> 24) & 0x0f),
		utf_buffer[1] = 0x80 | ((u >> 18) & 0x3f),
		utf_buffer[2] = 0x80 | ((u >> 12) & 0x3f),
		utf_buffer[3] = 0x80 | ((u >> 6) & 0x3f),
		utf_buffer[4] = 0x80 | (u & 0x3f);
	else	utf_buffer[0] = 0xfc | ((u >> 30) & 0x01),
		utf_buffer[1] = 0x80 | ((u >> 24) & 0x3f),
		utf_buffer[2] = 0x80 | ((u >> 18) & 0x3f),
		utf_buffer[3] = 0x80 | ((u >> 12) & 0x3f),
		utf_buffer[4] = 0x80 | ((u >> 6) & 0x3f),
		utf_buffer[5] = 0x80 | (u & 0x3f);

	return utf_buffer;
}

#ifdef CONFIG_UTF8
/* Number of bytes utf8 character indexed by first byte. Illegal bytes are
 * equal ones and handled different. */
static char utf8char_len_tab[256] = {
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 5,5,5,5,6,6,1,1,
};

inline int utf8charlen(const unsigned char *p)
{
	return p ? utf8char_len_tab[*p] : 0;
}

inline int
strlen_utf8(unsigned char **str)
{
	unsigned char *s = *str;
	unsigned char *end = strchr(s, '\0');
	int x;
	int len;

	for (x = 0;; x++, s += len) {
		len = utf8charlen(s);
		if (s + len > end) break;
	}
	*str = s;
	return x;
}

#define utf8_issingle(p) (((p) & 0x80) == 0)
#define utf8_islead(p) (utf8_issingle(p) || ((p) & 0xc0) == 0xc0)

/* Start from @current and move back to @pos char. This pointer return. The
 * most left pointer is @start. */
inline unsigned char *
utf8_prevchar(unsigned char *current, int pos, unsigned char *start)
{
	if (current == NULL || start == NULL || pos < 0)
		return NULL;
	while (pos > 0 && current != start) {
		current--;
		if (utf8_islead(*current))
			pos--;
	}
	return current;
}

/* Count number of standard terminal cells needed for displaying UTF-8
 * character. */
int
utf8_char2cells(unsigned char *utf8_char, unsigned char *end)
{
	unicode_val_T u;

	if (end == NULL)
		end = strchr(utf8_char, '\0');

	if(!utf8_char || !end)
		return -1;

	u = utf8_to_unicode(&utf8_char, end);

	return unicode_to_cell(u);
}

/* Count number of standard terminal cells needed for displaying string
 * with UTF-8 characters. */
int
utf8_ptr2cells(unsigned char *string, unsigned char *end)
{
	int charlen, cell, cells = 0;

	if (end == NULL)
		end = strchr(string, '\0');

	if(!string || !end)
		return -1;

	do {
		charlen = utf8charlen(string);
		if (string + charlen > end)
			break;

		cell = utf8_char2cells(string, end);
		if  (cell < 0)
			return -1;

		cells += cell;
		string += charlen;
	} while (1);

	return cells;
}

/* Count number of characters in string. */
int
utf8_ptr2chars(unsigned char *string, unsigned char *end)
{
	int charlen, chars = 0;

	if (end == NULL)
		end = strchr(string, '\0');

	if(!string || !end)
		return -1;

	do {
		charlen = utf8charlen(string);
		if (string + charlen > end)
			break;

		chars++;
		string += charlen;
	} while (1);

	return chars;
}

/*
 * Count number of bytes from begining of the string needed for displaying
 * specified number of cells.
 */
int
utf8_cells2bytes(unsigned char *string, int max_cells, unsigned char *end)
{
	unsigned int bytes = 0, cells = 0;

	assert(max_cells>=0);

	if (end == NULL)
		end = strchr(string, '\0');

	if(!string || !end)
		return -1;

	do {
		int cell = utf8_char2cells(&string[bytes], end);
		if (cell < 0)
			return -1;

		cells += cell;
		if (cells > max_cells)
			break;

		bytes += utf8charlen(&string[bytes]);

		if (string + bytes > end) {
			bytes = end - string;
			break;
		}
	} while(1);

	return bytes;
}

/* Take @max steps forward from @string in the specified @way, but
 * not going past @end.  Return the resulting address.  Store the
 * number of steps taken to *@count, unless @count is NULL.
 *
 * This assumes the text is valid UTF-8, and @string and @end point to
 * character boundaries.  If not, it doesn't crash but the results may
 * be inconsistent.
 *
 * This function can do some of the same jobs as utf8charlen(),
 * utf8_cells2bytes(), and strlen_utf8().  */
unsigned char *
utf8_step_forward(unsigned char *string, unsigned char *end,
		  int max, enum utf8_step way, int *count)
{
	int steps = 0;
	unsigned char *current = string;

	assert(string);
	assert(max >= 0);
	if_assert_failed goto invalid_arg;
	if (end == NULL)
		end = strchr(string, '\0');

	switch (way) {
	case utf8_step_characters:
		while (steps < max && current < end) {
			++current;
			if (utf8_islead(*current))
				++steps;
		}
		break;

	case utf8_step_cells_fewer:
	case utf8_step_cells_more:
		while (steps < max) {
			unicode_val_T u;
			unsigned char *prev = current;
			int width;

			u = utf8_to_unicode(&current, end);
			if (u == UCS_NO_CHAR) {
				/* Assume the incomplete sequence
				 * costs one cell.  */
				current = end;
				++steps;
				break;
			}

			width = unicode_to_cell(u);
			if (way == utf8_step_cells_fewer
			    && steps + width > max) {
				/* Back off.  */
				current = prev;
				break;
			}
			steps += width;
		}
		break;

	default:
		INTERNAL("impossible enum utf8_step");
	}

invalid_arg:
	if (count)
		*count = steps;
	return current;
}

/* Take @max steps backward from @string in the specified @way, but
 * not going past @start.  Return the resulting address.  Store the
 * number of steps taken to *@count, unless @count is NULL.
 *
 * This assumes the text is valid UTF-8, and @string and @start point
 * to character boundaries.  If not, it doesn't crash but the results
 * may be inconsistent.
 *
 * This function can do some of the same jobs as utf8_prevchar().  */
unsigned char *
utf8_step_backward(unsigned char *string, unsigned char *start,
		   int max, enum utf8_step way, int *count)
{
	int steps = 0;
	unsigned char *current = string;

	assert(string);
	assert(start);
	assert(max >= 0);
	if_assert_failed goto invalid_arg;

	switch (way) {
	case utf8_step_characters:
		while (steps < max && current > start) {
			--current;
			if (utf8_islead(*current))
				++steps;
		}
		break;

	case utf8_step_cells_fewer:
	case utf8_step_cells_more:
		while (steps < max) {
			unsigned char *prev = current;
			unsigned char *look;
			unicode_val_T u;
			int width;

			if (current <= start)
				break;
			do {
				--current;
			} while (current > start && !utf8_islead(*current));

			look = current;
			u = utf8_to_unicode(&look, prev);
			if (u == UCS_NO_CHAR) {
				/* Assume the incomplete sequence
				 * costs one cell.  */
				width = 1;
			} else
				width = unicode_to_cell(u);

			if (way == utf8_step_cells_fewer
			    && steps + width > max) {
				/* Back off.  */
				current = prev;
				break;
			}
			steps += width;
		}
		break;

	default:
		INTERNAL("impossible enum utf8_step");
	}

invalid_arg:
	if (count)
		*count = steps;
	return current;
}

/*
 * Find out number of standard terminal collumns needed for displaying symbol
 * (glyph) which represents Unicode character c.
 * TODO: Use wcwidth when it is available.
 *
 * @return	2 for double-width glyph, 1 for others.
 * 		TODO: May be extended to return 0 for zero-width glyphs
 * 		(like composing, maybe unprintable too).
 */
inline int
unicode_to_cell(unicode_val_T c)
{
	if (c >= 0x1100
		&& (c <= 0x115f			/* Hangul Jamo */
		|| c == 0x2329
		|| c == 0x232a
		|| (c >= 0x2e80 && c <= 0xa4cf
			&& c != 0x303f)		/* CJK ... Yi */
		|| (c >= 0xac00 && c <= 0xd7a3)	/* Hangul Syllables */
		|| (c >= 0xf900 && c <= 0xfaff)	/* CJK Compatibility
								Ideographs */
		|| (c >= 0xfe30 && c <= 0xfe6f)	/* CJK Compatibility Forms */
		|| (c >= 0xff00 && c <= 0xff60)	/* Fullwidth Forms */
		|| (c >= 0xffe0 && c <= 0xffe6)
		|| (c >= 0x20000 && c <= 0x2fffd)
		|| (c >= 0x30000 && c <= 0x3fffd)))
		return 2;

	return 1;
}

/* Fold the case of a Unicode character, so that hotkeys in labels can
 * be compared case-insensitively.  It is unspecified whether the
 * result will be in upper or lower case.  */
unicode_val_T
unicode_fold_label_case(unicode_val_T c)
{
#if __STDC_ISO_10646__ && HAVE_WCTYPE_H
	return towlower(c);
#else  /* !(__STDC_ISO_10646__ && HAVE_WCTYPE_H) */
	/* For now, this supports only ASCII.  It would be possible to
	 * use code generated from CaseFolding.txt of Unicode if the
	 * acknowledgements required by http://www.unicode.org/copyright.html
	 * were added to associated documentation of ELinks.  */
	if (c >= 0x41 && c <= 0x5A)
		return c + 0x20;
	else
		return c;
#endif /* !(__STDC_ISO_10646__ && HAVE_WCTYPE_H) */
}

inline unicode_val_T
utf8_to_unicode(unsigned char **string, unsigned char *end)
{
	unsigned char *str = *string;
	unicode_val_T u;
	int length;

	length = utf8char_len_tab[str[0]];

	if (str + length > end) {
		return UCS_NO_CHAR;
	}

	switch (length) {
		case 1:
			u = str[0];
			break;
		case 2:
			u = (str[0] & 0x1f) << 6;
			u += (str[1] & 0x3f);
			break;
		case 3:
			u = (str[0] & 0x0f) << 12;
			u += ((str[1] & 0x3f) << 6);
			u += (str[2] & 0x3f);
			break;
		case 4:
			u = (str[0] & 0x0f) << 18;
			u += ((str[1] & 0x3f) << 12);
			u += ((str[2] & 0x3f) << 6);
			u += (str[3] & 0x3f);
			break;
		case 5:
			u = (str[0] & 0x0f) << 24;
			u += ((str[1] & 0x3f) << 18);
			u += ((str[2] & 0x3f) << 12);
			u += ((str[3] & 0x3f) << 6);
			u += (str[4] & 0x3f);
			break;
		case 6:
		default:
			u = (str[0] & 0x01) << 30;
			u += ((str[1] & 0x3f) << 24);
			u += ((str[2] & 0x3f) << 18);
			u += ((str[3] & 0x3f) << 12);
			u += ((str[4] & 0x3f) << 6);
			u += (str[5] & 0x3f);
			break;
	}
	*string = str + length;
	return u;
}
#endif /* CONFIG_UTF8 */

/* Slow algorithm, the common part of cp2u and cp2utf8.  */
static unicode_val_T
cp2u_shared(const struct codepage_desc *from, unsigned char c)
{
	int j;

	for (j = 0; from->table[j].c; j++)
		if (from->table[j].c == c)
			return from->table[j].u;

	return UCS_REPLACEMENT_CHARACTER;
}

/* Slow algorithm, used for converting input from the terminal.  */
unicode_val_T
cp2u(int from, unsigned char c)
{
	from &= ~SYSTEM_CHARSET_FLAG;

	/* UTF-8 is a multibyte codepage and cannot be handled with
	 * this function.  */
	assert(codepages[from].table != table_utf8);
	if_assert_failed return UCS_REPLACEMENT_CHARACTER;

	if (c < 0x80) return c;
	else return cp2u_shared(&codepages[from], c);
}

/* This slow and ugly code is used by the terminal utf_8_io */
unsigned char *
cp2utf8(int from, int c)
{
	from &= ~SYSTEM_CHARSET_FLAG;

	if (codepages[from].table == table_utf8 || c < 128)
		return strings[c];

	return encode_utf8(cp2u_shared(&codepages[from], c));
}

#ifdef CONFIG_UTF8
unicode_val_T
cp_to_unicode(int codepage, unsigned char **string, unsigned char *end)
{
	unicode_val_T ret;

	if (is_cp_utf8(codepage))
		return utf8_to_unicode(string, end);

	if (*string >= end)
		return UCS_NO_CHAR;

	ret = cp2u(codepage, **string);
	++*string;
	return ret;
}
#endif	/* CONFIG_UTF8 */


static void
add_utf8(struct conv_table *ct, unicode_val_T u, unsigned char *str)
{
	unsigned char *p = encode_utf8(u);

	while (p[1]) {
		if (ct[*p].t) ct = ct[*p].u.tbl;
		else {
			struct conv_table *nct;

			assertm(ct[*p].u.str == no_str, "bad utf encoding #1");
			if_assert_failed return;

			nct = mem_calloc(256, sizeof(*nct));
			if (!nct) return;
			new_translation_table(nct);
			ct[*p].t = 1;
			ct[*p].u.tbl = nct;
			ct = nct;
		}
		p++;
	}

	assertm(!ct[*p].t, "bad utf encoding #2");
	if_assert_failed return;

	if (ct[*p].u.str == no_str)
		ct[*p].u.str = str;
}

struct conv_table utf_table[256];
int utf_table_init = 1;

static void
free_utf_table(void)
{
	int i;

	for (i = 128; i < 256; i++)
		mem_free(utf_table[i].u.str);
}

static struct conv_table *
get_translation_table_to_utf8(int from)
{
	int i;
	static int lfr = -1;

	if (from == -1) return NULL;
	from &= ~SYSTEM_CHARSET_FLAG;
	if (from == lfr) return utf_table;
	lfr = from;
	if (utf_table_init)
		memset(utf_table, 0, sizeof(utf_table)),
		utf_table_init = 0;
	else
		free_utf_table();

	for (i = 0; i < 128; i++)
		utf_table[i].u.str = strings[i];

	if (codepages[from].table == table_utf8) {
		for (i = 128; i < 256; i++)
			utf_table[i].u.str = stracpy(strings[i]);
		return utf_table;
	}

	for (i = 128; i < 256; i++)
		utf_table[i].u.str = NULL;

	for (i = 0; codepages[from].table[i].c; i++) {
		unicode_val_T u = codepages[from].table[i].u;

		if (!utf_table[codepages[from].table[i].c].u.str)
			utf_table[codepages[from].table[i].c].u.str =
				stracpy(encode_utf8(u));
	}

	for (i = 128; i < 256; i++)
		if (!utf_table[i].u.str)
			utf_table[i].u.str = stracpy(no_str);

	return utf_table;
}

struct conv_table table[256];
static int first = 1;

void
free_conv_table(void)
{
	if (!utf_table_init) free_utf_table();
	if (first) {
		memset(table, 0, sizeof(table));
		first = 0;
	}
	new_translation_table(table);
}


struct conv_table *
get_translation_table(int from, int to)
{
	static int lfr = -1;
	static int lto = -1;

	from &= ~SYSTEM_CHARSET_FLAG;
	to &= ~SYSTEM_CHARSET_FLAG;
	if (first) {
		memset(table, 0, sizeof(table));
		first = 0;
	}
	if (/*from == to ||*/ from == -1 || to == -1)
		return NULL;
	if (codepages[to].table == table_utf8)
		return get_translation_table_to_utf8(from);
	if (from == lfr && to == lto)
		return table;
	lfr = from;
	lto = to;
	new_translation_table(table);

	if (codepages[from].table == table_utf8) {
		int i;

		for (i = 0; codepages[to].table[i].c; i++)
			add_utf8(table, codepages[to].table[i].u,
				 strings[codepages[to].table[i].c]);

		for (i = 0; unicode_7b[i].x != -1; i++)
			if (unicode_7b[i].x >= 0x80)
				add_utf8(table, unicode_7b[i].x,
					 unicode_7b[i].s);

	} else {
		int i;

		for (i = 128; i < 256; i++) {
			int j;

			for (j = 0; codepages[from].table[j].c; j++) {
				if (codepages[from].table[j].c == i) {
					unsigned char *u;

					u = u2cp(codepages[from].table[j].u, to);
					if (u) table[i].u.str = u;
					break;
				}
			}
		}
	}

	return table;
}

static inline int
xxstrcmp(unsigned char *s1, unsigned char *s2, int l2)
{
	while (l2) {
		if (*s1 > *s2) return 1;
		if (*s1 < *s2) return -1;
		s1++;
	       	s2++;
		l2--;
	}

	return *s2 ? -1 : 0;
}

/* Entity cache debugging purpose. */
#if 0
#define DEBUG_ENTITY_CACHE
#else
#undef DEBUG_ENTITY_CACHE
#endif

struct entity_cache {
	unsigned int hits;
	int strlen;
	int encoding;
	unsigned char *result;
	unsigned char str[20]; /* Suffice in any case. */
};

static int
hits_cmp(struct entity_cache *a, struct entity_cache *b)
{
	if (a->hits == b->hits) return 0;
	if (a->hits > b->hits) return -1;
	else return 1;
}

static int
compare_entities(const void *key_, const void *element_)
{
	struct string *key = (struct string *) key_;
	struct entity *element = (struct entity *) element_;
	int length = key->length;
	unsigned char *first = key->source;
	unsigned char *second = element->s;

	return xxstrcmp(first, second, length);
}

unsigned char *
get_entity_string(const unsigned char *str, const int strlen, int encoding)
{
#define ENTITY_CACHE_SIZE 10	/* 10 seems a good value. */
#define ENTITY_CACHE_MAXLEN 9   /* entities with length >= ENTITY_CACHE_MAXLEN or == 1
			           will go in [0] table */
	static struct entity_cache entity_cache[ENTITY_CACHE_MAXLEN][ENTITY_CACHE_SIZE];
	static unsigned int nb_entity_cache[ENTITY_CACHE_MAXLEN];
	static int first_time = 1;
	unsigned int slen = 0;
	unsigned char *result = NULL;

	if (strlen <= 0) return NULL;

#ifdef CONFIG_UTF8
	/* TODO: caching UTF-8 */
	encoding &= ~SYSTEM_CHARSET_FLAG;
	if (codepages[encoding].table == table_utf8)
		goto skip;
#endif /* CONFIG_UTF8 */

	if (first_time) {
		memset(&nb_entity_cache, 0, ENTITY_CACHE_MAXLEN * sizeof(unsigned int));
		first_time = 0;
	}

	/* Check if cached. A test on many websites (freshmeat.net + whole ELinks website
	 * + google + slashdot + websites that result from a search for test on google,
	 * + various ones) show a quite impressive improvment:
	 * Top ten is:
	 * 0: hits=2459 l=4 st='nbsp'
	 * 1: hits=2152 l=6 st='eacute'
	 * 2: hits=235 l=6 st='egrave'
	 * 3: hits=136 l=6 st='agrave'
	 * 4: hits=100 l=3 st='amp'
	 * 5: hits=40 l=5 st='laquo'
	 * 6: hits=8 l=4 st='copy'
	 * 7: hits=5 l=2 st='gt'
	 * 8: hits=2 l=2 st='lt'
	 * 9: hits=1 l=6 st='middot'
	 *
	 * Most of the time cache hit ratio is near 95%.
	 *
	 * A long test shows: 15186 hits vs. 24 misses and mean iteration
	 * count is kept < 2 (worst case 1.58). Not so bad ;)
	 *
	 * --Zas */

	/* entities with length >= ENTITY_CACHE_MAXLEN or == 1 will go in [0] table */
	slen = (strlen > 1 && strlen < ENTITY_CACHE_MAXLEN) ? strlen : 0;

	if (strlen < ENTITY_CACHE_MAXLEN && nb_entity_cache[slen] > 0) {
		int i;

		for (i = 0; i < nb_entity_cache[slen]; i++) {
			if (entity_cache[slen][i].encoding == encoding
			    && !memcmp(str, entity_cache[slen][i].str, strlen)) {
#ifdef DEBUG_ENTITY_CACHE
				static double total_iter = 0;
				static unsigned long hit_count = 0;

				total_iter += i + 1;
				hit_count++;
				fprintf(stderr, "hit after %d iter. (mean = %0.2f)\n", i + 1, total_iter / (double) hit_count);
#endif
				if (entity_cache[slen][i].hits < (unsigned int) ~0)
					entity_cache[slen][i].hits++;
				return entity_cache[slen][i].result;
			}
		}
#ifdef DEBUG_ENTITY_CACHE
		fprintf(stderr, "miss\n");
#endif
	}
#ifdef CONFIG_UTF8
skip:
#endif /* CONFIG_UTF8 */
	if (*str == '#') { /* Numeric entity. */
		int l = (int) strlen;
		unsigned char *st = (unsigned char *) str;
		unicode_val_T n = 0;

		if (l == 1) goto end; /* &#; ? */
		st++, l--;
		if ((*st | 32) == 'x') { /* Hexadecimal */

			if (l == 1 || l > 9) goto end; /* xFFFFFFFF max. */
			st++, l--;
			do {
				unsigned char c = (*(st++) | 32);

				if (isdigit(c))
					n = (n << 4) | (c - '0');
				else if (isxdigit(c))
					n = (n << 4) | (c - 'a' + 10);
				else
					goto end; /* Bad char. */
			} while (--l);
		} else { /* Decimal */
			if (l > 10) goto end; /* 4294967295 max. */
			do {
				unsigned char c = *(st++);

				if (isdigit(c))
					n = n * 10 + c - '0';
				else
					goto end; /* Bad char. */
				/* Limit to 0xFFFFFFFF. */
				if (n >= (unicode_val_T) 0xFFFFFFFFu)
					goto end;
			} while (--l);
		}

		result = u2cp(n, encoding);

#ifdef DEBUG_ENTITY_CACHE
		fprintf(stderr, "%lu %016x %s\n", (unsigned long) n , n, result);
#endif
	} else { /* Text entity. */
		struct string key = INIT_STRING((unsigned char *) str, strlen);
		struct entity *element = bsearch((void *) &key, entities,
						 N_ENTITIES,
						 sizeof(*element),
						 compare_entities);

		if (element) result = u2cp(element->c, encoding);
	}

#ifdef CONFIG_UTF8
	if (codepages[encoding].table == table_utf8) {
		return result;
	}
#endif /* CONFIG_UTF8 */
end:
	/* Take care of potential buffer overflow. */
	if (strlen < sizeof(entity_cache[slen][0].str)) {
		struct entity_cache *ece = &entity_cache[slen][nb_entity_cache[slen]];

		/* Copy new entry to cache. */
		ece->hits = 1;
		ece->strlen = strlen;
		ece->encoding = encoding;
		ece->result = result;
		memcpy(ece->str, str, strlen);
		ece->str[strlen] = '\0';

		/* Increment number of cache entries if possible. */
		if (nb_entity_cache[slen] < ENTITY_CACHE_SIZE) nb_entity_cache[slen]++;

#ifdef DEBUG_ENTITY_CACHE
		fprintf(stderr, "Added in [%u]: l=%d st='%s'\n", slen,
				entity_cache[slen][0].strlen, entity_cache[slen][0].str);

#endif

		/* Sort entries by hit order. */
		if (nb_entity_cache[slen] > 1)
			qsort(&entity_cache[slen][0], nb_entity_cache[slen],
			      sizeof(entity_cache[slen][0]), (void *) hits_cmp);

#ifdef DEBUG_ENTITY_CACHE
	{
		unsigned int i;

		fprintf(stderr, "- Cache entries [%u] -\n", slen);
		for (i = 0; i < nb_entity_cache[slen] ; i++)
			fprintf(stderr, "%d: hits=%u l=%d st='%s'\n", i,
				entity_cache[slen][i].hits, entity_cache[slen][i].strlen,
				entity_cache[slen][i].str);
		fprintf(stderr, "-----------------\n");
	}
#endif
	}
	return result;
}

unsigned char *
convert_string(struct conv_table *convert_table,
	       unsigned char *chars, int charslen, int cp,
	       enum convert_string_mode mode, int *length,
	       void (*callback)(void *data, unsigned char *buf, int buflen),
	       void *callback_data)
{
	unsigned char *buffer;
	int bufferpos = 0;
	int charspos = 0;

	if (!convert_table && !memchr(chars, '&', charslen)) {
		if (callback) {
			if (charslen) callback(callback_data, chars, charslen);
			return NULL;
		} else {
			return memacpy(chars, charslen);
		}
	}

	/* Buffer allocation */

	buffer = mem_alloc(ALLOC_GR + 1 /* trailing \0 */);
	if (!buffer) return NULL;

	/* Iterate ;-) */

	while (charspos < charslen) {
		unsigned char *translit;

#define PUTC do { \
		buffer[bufferpos++] = chars[charspos++]; \
		translit = ""; \
		goto flush; \
	} while (0)

		if (chars[charspos] != '&') {
			struct conv_table *t;
			int i;

			if (chars[charspos] < 128 || !convert_table) PUTC;

			t = convert_table;
			i = charspos;

			while (t[chars[i]].t) {
				t = t[chars[i++]].u.tbl;
				if (i >= charslen) PUTC;
			}

			translit = t[chars[i]].u.str;
			charspos = i + 1;

		} else if (mode == CSM_FORM || mode == CSM_NONE) {
			PUTC;

		} else {
			int start = charspos + 1;
			int i = start;

			while (i < charslen
			       && (isasciialpha(chars[i])
				   || isdigit(chars[i])
				   || (chars[i] == '#')))
				i++;

			/* This prevents bug 213: we were expanding "entities"
			 * in URL query strings. */
			/* XXX: But this disables &nbsp&nbsp usage, which
			 * appears to be relatively common! --pasky */
			if ((mode == CSM_DEFAULT || (chars[i] != '&' && chars[i] != '='))
			    && i > start
			    && !isasciialpha(chars[i]) && !isdigit(chars[i])) {
				translit = get_entity_string(&chars[start], i - start,
						      cp);
				if (chars[i] != ';') {
					/* Eat &nbsp &nbsp<foo> happily, but
					 * pull back from the character after
					 * entity string if it is not the valid
					 * terminator. */
					i--;
				}

				if (!translit) PUTC;
				charspos = i + (i < charslen);
			} else PUTC;
		}

		if (!translit[0]) continue;

		if (!translit[1]) {
			buffer[bufferpos++] = translit[0];
			translit = "";
			goto flush;
		}

		while (*translit) {
			unsigned char *new;

			buffer[bufferpos++] = *(translit++);
flush:
			if (bufferpos & (ALLOC_GR - 1)) continue;

			if (callback) {
				buffer[bufferpos] = 0;
				callback(callback_data, buffer, bufferpos);
				bufferpos = 0;
			} else {
				new = mem_realloc(buffer, bufferpos + ALLOC_GR);
				if (!new) {
					mem_free(buffer);
					return NULL;
				}
				buffer = new;
			}
		}
#undef PUTC
	}

	/* Say bye */

	buffer[bufferpos] = 0;
	if (length) *length = bufferpos;

	if (callback) {
		if (bufferpos) callback(callback_data, buffer, bufferpos);
		mem_free(buffer);
		return NULL;
	} else {
		return buffer;
	}
}


#ifndef USE_FASTFIND
int
get_cp_index(unsigned char *name)
{
	int i, a;
	int syscp = 0;

	if (!strcasecmp(name, "System")) {
#if HAVE_LANGINFO_CODESET
		name = nl_langinfo(CODESET);
		syscp = SYSTEM_CHARSET_FLAG;
#else
		name = "us-ascii";
#endif
	}

	for (i = 0; codepages[i].name; i++) {
		for (a = 0; codepages[i].aliases[a]; a++) {
			/* In the past, we looked for the longest substring
			 * in all the names; it is way too expensive, though:
			 *
			 *   %   cumulative   self              self     total
			 *  time   seconds   seconds    calls  us/call  us/call  name
			 *  3.00      0.66     0.03     1325    22.64    22.64  get_cp_index
			 *
			 * Anything called from redraw_screen() is in fact
			 * relatively expensive, even if it's called just
			 * once. So we will do a simple strcasecmp() here.
			 */

			if (!strcasecmp(name, codepages[i].aliases[a]))
				return i | syscp;
		}
	}

	if (syscp) {
		return get_cp_index("us-ascii") | syscp;
	} else {
		return -1;
	}
}

#else

static unsigned int i_name = 0;
static unsigned int i_alias = 0;

/* Reset internal list pointer */
void
charsets_list_reset(void)
{
	i_name = 0;
	i_alias = 0;
}

/* Returns a pointer to a struct that contains current key and data pointers
 * and increment internal pointer.  It returns NULL when key is NULL. */
struct fastfind_key_value *
charsets_list_next(void)
{
	static struct fastfind_key_value kv;

	if (!codepages[i_name].name) return NULL;

	kv.key = codepages[i_name].aliases[i_alias];
	kv.data = (void *) &codepages[i_name]; /* cast away const */

	if (codepages[i_name].aliases[i_alias + 1])
		i_alias++;
	else {
		i_name++;
		i_alias = 0;
	}

	return &kv;
}

static struct fastfind_index ff_charsets_index
	= INIT_FASTFIND_INDEX("charsets_lookup", charsets_list_reset, charsets_list_next);

/* It searchs for a charset named @name or one of its aliases and
 * returns index for it or -1 if not found. */
int
get_cp_index(unsigned char *name)
{
	const struct codepage_desc *codepage;
	int syscp = 0;

	if (!strcasecmp(name, "System")) {
#if HAVE_LANGINFO_CODESET
		name = nl_langinfo(CODESET);
		syscp = SYSTEM_CHARSET_FLAG;
#else
		name = "us-ascii";
#endif
	}

	codepage = fastfind_search(&ff_charsets_index, name, strlen(name));
	if (codepage) {
		assert(codepages <= codepage && codepage < codepages + N_CODEPAGES);
		return (codepage - codepages) | syscp;

	} else if (syscp) {
		return get_cp_index("us-ascii") | syscp;

	} else {
		return -1;
	}
}

#endif /* USE_FASTFIND */

void
init_charsets_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_index(&ff_charsets_index, FF_COMPRESS);
#endif
}

void
free_charsets_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_done(&ff_charsets_index);
#endif
}

unsigned char *
get_cp_name(int cp_index)
{
	if (cp_index < 0) return "none";
	if (cp_index & SYSTEM_CHARSET_FLAG) return "System";

	return codepages[cp_index].name;
}

unsigned char *
get_cp_mime_name(int cp_index)
{
	if (cp_index < 0) return "none";
	if (cp_index & SYSTEM_CHARSET_FLAG) return "System";
	if (!codepages[cp_index].aliases) return NULL;

	return codepages[cp_index].aliases[0];
}

int
is_cp_utf8(int cp_index)
{
	cp_index &= ~SYSTEM_CHARSET_FLAG;
	return codepages[cp_index].table == table_utf8;
}
