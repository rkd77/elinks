/* Charsets convertor */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasecmp() */
#endif

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

 	/* The Unicode mappings of codepage bytes 0x80...0xFF.
 	 * (0x00...0x7F are assumed to be ASCII in all codepages.)
 	 * Because all current values fit in 16 bits, we store them as
 	 * uint16_t rather than unicode_val_T.  If the codepage does
 	 * not use some byte, then @highhalf maps that byte to 0xFFFF,
 	 * which C code converts to UCS_REPLACEMENT_CHARACTER where
 	 * appropriate.  (U+FFFF is reserved and will never be
 	 * assigned as a character.)  */
	const uint16_t *highhalf;

 	/* If some byte in the codepage corresponds to multiple Unicode
 	 * characters, then the preferred character is in @highhalf
 	 * above, and the rest are listed here in @table.  This table
 	 * is not used for translating from the codepage to Unicode.  */
	const struct table_entry *table;
};

#include "intl/codepage.inc"
#include "intl/uni_7b.inc"
#include "intl/entity.inc"

/* Declare the external-linkage inline functions defined in this file.
 * Avoid the GCC 4.3.1 warning: `foo' declared inline after being
 * called.  The functions are not declared inline in charsets.h
 * because C99 6.7.4p6 says that every external-linkage function
 * declared inline shall be defined in the same translation unit.
 * The non-inline declarations in charsets.h also make sure that the
 * compiler emits global definitions for the symbols so that the
 * functions can be called from other translation units.  */
NONSTATIC_INLINE unsigned char *encode_utf8(unicode_val_T u);
NONSTATIC_INLINE int utf8charlen(const unsigned char *p);
NONSTATIC_INLINE int unicode_to_cell(unicode_val_T c);
NONSTATIC_INLINE unicode_val_T utf8_to_unicode(unsigned char **string,
					       const unsigned char *end);

static const char strings[256][2] = {
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

/* A string used in conversion tables when there is no correct
 * conversion.  This is compared by address and therefore should be a
 * named array rather than a pointer so that it won't share storage
 * with any other string literal that happens to have the same
 * characters.  */
static const unsigned char no_str[] = "*";

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
#define is_cp_ptr_utf8(cp_ptr) ((cp_ptr)->aliases == aliases_utf8)

const unsigned char *
u2cp_(unicode_val_T u, int to, enum nbsp_mode nbsp_mode)
{
	int j;
	int s;

	if (u < 128) return strings[u];

	if (u < 0xa0) {
		u = strange_chars[u - 0x80];
		if (!u) return NULL;
	}

	to &= ~SYSTEM_CHARSET_FLAG;

	if (is_cp_ptr_utf8(&codepages[to]))
		return encode_utf8(u);

	/* To mark non breaking spaces in non-UTF-8 strings, we use a
	 * special char NBSP_CHAR. */
	if (u == UCS_NO_BREAK_SPACE) {
		if (nbsp_mode == NBSP_MODE_HACK) return NBSP_CHAR_STRING;
		else /* NBSP_MODE_ASCII */ return " ";
	}
	if (u == UCS_SOFT_HYPHEN) return "";

	if (u < 0xFFFF)
		for (j = 0; j < 0x80; j++)
			if (codepages[to].highhalf[j] == u)
				return strings[0x80 + j];
	for (j = 0; codepages[to].table[j].c; j++)
		if (codepages[to].table[j].u == u)
			return strings[codepages[to].table[j].c];

	BIN_SEARCH(unicode_7b, x, N_UNICODE_7B, u, s);
	if (s != -1) return unicode_7b[s].s;

	return no_str;
}

static unsigned char utf_buffer[7];

NONSTATIC_INLINE unsigned char *
encode_utf8(unicode_val_T u)
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

/* Number of bytes utf8 character indexed by first byte. Illegal bytes are
 * equal ones and handled different. */
static const char utf8char_len_tab[256] = {
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
	3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 5,5,5,5,6,6,1,1,
};

#ifdef CONFIG_UTF8
NONSTATIC_INLINE int
utf8charlen(const unsigned char *p)
{
	return p ? utf8char_len_tab[*p] : 0;
}

int
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
unsigned char *
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
	case UTF8_STEP_CHARACTERS:
		while (steps < max && current < end) {
			++current;
			if (utf8_islead(*current))
				++steps;
		}
		break;

	case UTF8_STEP_CELLS_FEWER:
	case UTF8_STEP_CELLS_MORE:
		while (steps < max && current < end) {
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
			if (way == UTF8_STEP_CELLS_FEWER
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
	case UTF8_STEP_CHARACTERS:
		while (steps < max && current > start) {
			--current;
			if (utf8_islead(*current))
				++steps;
		}
		break;

	case UTF8_STEP_CELLS_FEWER:
	case UTF8_STEP_CELLS_MORE:
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

			if (way == UTF8_STEP_CELLS_FEWER
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
 *
 * TODO: Use wcwidth when it is available. This seems to require:
 * - Make the configure script check whether <wchar.h> and wcwidth exist.
 * - Define _XOPEN_SOURCE and include <wchar.h>.
 * - Test that __STDC_ISO_10646__ is defined.  (This macro means wchar_t
 *   matches ISO 10646 in all locales.)
 * However, these do not suffice, because wcwidth depends on LC_CTYPE
 * in glibc-2.3.6.  For instance, wcwidth(0xff20) is -1 when LC_CTYPE
 * is "fi_FI.ISO-8859-1" or "C", but 2 when LC_CTYPE is "fi_FI.UTF-8".
 * <features.h> defines __STDC_ISO_10646__ as 200009L, so 0xff20 means
 * U+FF20 FULLWIDTH COMMERCIAL AT regardless of LC_CTYPE; but this
 * character is apparently not supported in all locales.  Why is that?
 * - Perhaps there is standardese that requires supported characters
 *   to be convertable to multibyte form.  Then ELinks could just pick
 *   some UTF-8 locale for its wcwidth purposes.
 * - Perhaps wcwidth can even return different nonnegative values for
 *   the same ISO 10646 character in different locales.  Then ELinks
 *   would have to set LC_CTYPE to match at least the terminal's
 *   charset (which may differ from the LC_CTYPE environment variable,
 *   especially when the master process is serving a slave terminal).
 *   But there is no guarantee that the libc supports all the same
 *   charsets as ELinks does.
 * For now, it seems safest to avoid the potentially locale-dependent
 * libc version of wcwidth, and instead use a hardcoded mapping.
 *
 * @return	2 for double-width glyph, 1 for others.
 * 		TODO: May be extended to return 0 for zero-width glyphs
 * 		(like composing, maybe unprintable too).
 */
NONSTATIC_INLINE int
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
#endif /* CONFIG_UTF8 */

NONSTATIC_INLINE unicode_val_T
utf8_to_unicode(unsigned char **string, const unsigned char *end)
{
	unsigned char *str = *string;
	unicode_val_T u;
	int length;

	length = utf8char_len_tab[str[0]];

	if (str + length > end) {
		return UCS_NO_CHAR;
	}

	switch (length) {
		case 1:		/* U+0000 to U+007F */
			if (str[0] >= 0x80) {
invalid_utf8:
				++*string;
				return UCS_REPLACEMENT_CHARACTER;
			}
			u = str[0];
			break;
		case 2:		/* U+0080 to U+07FF */
			if ((str[1] & 0xc0) != 0x80)
				goto invalid_utf8;
			u = (str[0] & 0x1f) << 6;
			u += (str[1] & 0x3f);
			if (u < 0x80)
				goto invalid_utf8;
			break;
		case 3:		/* U+0800 to U+FFFF, except surrogates */
			if ((str[1] & 0xc0) != 0x80 || (str[2] & 0xc0) != 0x80)
				goto invalid_utf8;
			u = (str[0] & 0x0f) << 12;
			u += ((str[1] & 0x3f) << 6);
			u += (str[2] & 0x3f);
			if (u < 0x800 || is_utf16_surrogate(u))
				goto invalid_utf8;
			break;
		case 4:		/* U+10000 to U+1FFFFF */
			if ((str[1] & 0xc0) != 0x80 || (str[2] & 0xc0) != 0x80
			    || (str[3] & 0xc0) != 0x80)
				goto invalid_utf8;
			u = (str[0] & 0x0f) << 18;
			u += ((str[1] & 0x3f) << 12);
			u += ((str[2] & 0x3f) << 6);
			u += (str[3] & 0x3f);
			if (u < 0x10000)
				goto invalid_utf8;
			break;
		case 5:		/* U+200000 to U+3FFFFFF */
			if ((str[1] & 0xc0) != 0x80 || (str[2] & 0xc0) != 0x80
			    || (str[3] & 0xc0) != 0x80 || (str[4] & 0xc0) != 0x80)
				goto invalid_utf8;
			u = (str[0] & 0x0f) << 24;
			u += ((str[1] & 0x3f) << 18);
			u += ((str[2] & 0x3f) << 12);
			u += ((str[3] & 0x3f) << 6);
			u += (str[4] & 0x3f);
			if (u < 0x200000)
				goto invalid_utf8;
			break;
		case 6:		/* U+4000000 to U+7FFFFFFF */
			if ((str[1] & 0xc0) != 0x80 || (str[2] & 0xc0) != 0x80
			    || (str[3] & 0xc0) != 0x80 || (str[4] & 0xc0) != 0x80
			    || (str[5] & 0xc0) != 0x80)
				goto invalid_utf8;
			u = (str[0] & 0x01) << 30;
			u += ((str[1] & 0x3f) << 24);
			u += ((str[2] & 0x3f) << 18);
			u += ((str[3] & 0x3f) << 12);
			u += ((str[4] & 0x3f) << 6);
			u += (str[5] & 0x3f);
			if (u < 0x4000000)
				goto invalid_utf8;
			break;
		default:
			INTERNAL("utf8char_len_tab out of range");
			goto invalid_utf8;
	}
	*string = str + length;
	return u;
}

/* The common part of cp2u and cp2utf_8.  */
static unicode_val_T
cp2u_shared(const struct codepage_desc *from, unsigned char c)
{
	unicode_val_T u = from->highhalf[c - 0x80];

	if (u == 0xFFFF) u = UCS_REPLACEMENT_CHARACTER;
	return u;
}

/* Used for converting input from the terminal.  */
unicode_val_T
cp2u(int from, unsigned char c)
{
	from &= ~SYSTEM_CHARSET_FLAG;

	/* UTF-8 is a multibyte codepage and cannot be handled with
	 * this function.  */
	assert(!is_cp_ptr_utf8(&codepages[from]));
	if_assert_failed return UCS_REPLACEMENT_CHARACTER;

	if (c < 0x80) return c;
	else return cp2u_shared(&codepages[from], c);
}

/* This slow and ugly code is used by the terminal utf_8_io */
const unsigned char *
cp2utf8(int from, int c)
{
	from &= ~SYSTEM_CHARSET_FLAG;

	if (is_cp_ptr_utf8(&codepages[from]) || c < 128)
		return strings[c];

	return encode_utf8(cp2u_shared(&codepages[from], c));
}

unicode_val_T
cp_to_unicode(int codepage, unsigned char **string, const unsigned char *end)
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


static void
add_utf8(struct conv_table *ct, unicode_val_T u, const unsigned char *str)
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

/* A conversion table from some charset to UTF-8.
 * If it is from UTF-8 to UTF-8, it converts each byte separately.
 * Unlike in other translation tables, the strings in elements 0x80 to
 * 0xFF are allocated dynamically.  */
struct conv_table utf_table[256];
int utf_table_init = 1;

static void
free_utf_table(void)
{
	int i;

	/* Cast away const.  */
	for (i = 128; i < 256; i++)
		mem_free((unsigned char *) utf_table[i].u.str);
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
	if (utf_table_init) {
		memset(utf_table, 0, sizeof(utf_table));
		utf_table_init = 0;
	} else
		free_utf_table();

	for (i = 0; i < 128; i++)
		utf_table[i].u.str = strings[i];

	if (is_cp_ptr_utf8(&codepages[from])) {
		for (i = 128; i < 256; i++)
			utf_table[i].u.str = stracpy(strings[i]);
		return utf_table;
	}

	for (i = 128; i < 256; i++) {
		unicode_val_T u = codepages[from].highhalf[i - 0x80];

		if (u == 0xFFFF)
			utf_table[i].u.str = NULL;
		else
			utf_table[i].u.str = stracpy(encode_utf8(u));
	}

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

/* A conversion table between two charsets, where the target is not UTF-8.  */
static struct conv_table table[256];
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
	if (is_cp_ptr_utf8(&codepages[to]))
		return get_translation_table_to_utf8(from);
	if (from == lfr && to == lto)
		return table;
	lfr = from;
	lto = to;
	new_translation_table(table);

	if (is_cp_ptr_utf8(&codepages[from])) {
		int i;

		/* Map U+00A0 and U+00AD the same way as u2cp() would.  */
		add_utf8(table, UCS_NO_BREAK_SPACE, strings[NBSP_CHAR]);
		add_utf8(table, UCS_SOFT_HYPHEN, "");

		for (i = 0x80; i <= 0xFF; i++)
			if (codepages[to].highhalf[i - 0x80] != 0xFFFF)
				add_utf8(table,
					 codepages[to].highhalf[i - 0x80],
					 strings[i]);

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
			if (codepages[from].highhalf[i - 0x80] != 0xFFFF) {
				const unsigned char *u;

				u = u2cp(codepages[from].highhalf[i - 0x80], to);
				if (u) table[i].u.str = u;
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
	const unsigned char *result;
	unsigned char str[20]; /* Suffice in any case. */
};

/* comparison function for qsort() */
static int
hits_cmp(const void *v1, const void *v2)
{
	const struct entity_cache *a = v1, *b = v2;

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

const unsigned char *
get_entity_string(const unsigned char *str, const int strlen, int encoding)
{
#define ENTITY_CACHE_SIZE 10	/* 10 seems a good value. */
#define ENTITY_CACHE_MAXLEN 9   /* entities with length >= ENTITY_CACHE_MAXLEN or == 1
			           will go in [0] table */
	static struct entity_cache entity_cache[ENTITY_CACHE_MAXLEN][ENTITY_CACHE_SIZE];
	static unsigned int nb_entity_cache[ENTITY_CACHE_MAXLEN];
	static int first_time = 1;
	unsigned int slen = 0;
	const unsigned char *result = NULL;

	if (strlen <= 0) return NULL;

#ifdef CONFIG_UTF8
	/* TODO: caching UTF-8 */
	encoding &= ~SYSTEM_CHARSET_FLAG;
	if (is_cp_ptr_utf8(&codepages[encoding]))
		goto skip;
#endif /* CONFIG_UTF8 */

	if (first_time) {
		memset(&nb_entity_cache, 0, ENTITY_CACHE_MAXLEN * sizeof(unsigned int));
		first_time = 0;
	}

	/* Check if cached. A test on many websites (freshmeat.net + whole ELinks website
	 * + google + slashdot + websites that result from a search for test on google,
	 * + various ones) show quite impressive improvment:
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
	if (is_cp_ptr_utf8(&codepages[encoding])) {
		return result;
	}
#endif /* CONFIG_UTF8 */
end:
	/* Take care of potential buffer overflow. */
	if (strlen < sizeof(entity_cache[slen][0].str)) {
		struct entity_cache *ece;

		/* Sort entries by hit order. */
		if (nb_entity_cache[slen] > 1)
			qsort(&entity_cache[slen][0], nb_entity_cache[slen],
			      sizeof(entity_cache[slen][0]), hits_cmp);

		/* Increment number of cache entries if possible.
		 * Else, just replace the least used entry.  */
		if (nb_entity_cache[slen] < ENTITY_CACHE_SIZE) nb_entity_cache[slen]++;
		ece = &entity_cache[slen][nb_entity_cache[slen] - 1];

		/* Copy new entry to cache. */
		ece->hits = 1;
		ece->strlen = strlen;
		ece->encoding = encoding;
		ece->result = result;
		memcpy(ece->str, str, strlen);
		ece->str[strlen] = '\0';


#ifdef DEBUG_ENTITY_CACHE
		fprintf(stderr, "Added in [%u]: l=%d st='%s'\n", slen,
				entity_cache[slen][0].strlen, entity_cache[slen][0].str);

	{
		unsigned int i;

		fprintf(stderr, "- Cache entries [%u] -\n", slen);
		for (i = 0; i < nb_entity_cache[slen] ; i++)
			fprintf(stderr, "%d: hits=%u l=%d st='%s'\n", i,
				entity_cache[slen][i].hits, entity_cache[slen][i].strlen,
				entity_cache[slen][i].str);
		fprintf(stderr, "-----------------\n");
	}
#endif	/* DEBUG_ENTITY_CACHE */
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
		const unsigned char *translit;

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
get_cp_index(const unsigned char *name)
{
	int i, a;
	int syscp = 0;

	if (!c_strcasecmp(name, "System")) {
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

			if (!c_strcasecmp(name, codepages[i].aliases[a]))
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
get_cp_index(const unsigned char *name)
{
	const struct codepage_desc *codepage;
	int syscp = 0;

	if (!c_strcasecmp(name, "System")) {
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

/* Get the codepage's name for displaying to the user, or NULL if
 * @cp_index is one past the end.  In the future, we might want to
 * localize these with gettext.  So it may be best not to use this
 * function if the name will have to be converted back to an
 * index.  */
unsigned char *
get_cp_name(int cp_index)
{
	if (cp_index < 0) return "none";
	if (cp_index & SYSTEM_CHARSET_FLAG) return "System";

	return codepages[cp_index].name;
}

/* Get the codepage's name for saving to a configuration file.  These
 * names can be converted back to indexes, even in future versions of
 * ELinks.  */
unsigned char *
get_cp_config_name(int cp_index)
{
	if (cp_index < 0) return "none";
	if (cp_index & SYSTEM_CHARSET_FLAG) return "System";
	if (!codepages[cp_index].aliases) return NULL;

	return codepages[cp_index].aliases[0];
}

/* Get the codepage's name for sending to a library or server that
 * understands MIME charset names.  This function irreversibly maps
 * the "System" codepage to the underlying charset.  */
unsigned char *
get_cp_mime_name(int cp_index)
{
	if (cp_index < 0) return "none";
	cp_index &= ~SYSTEM_CHARSET_FLAG;
	if (!codepages[cp_index].aliases) return NULL;

	return codepages[cp_index].aliases[0];
}

int
is_cp_utf8(int cp_index)
{
	cp_index &= ~SYSTEM_CHARSET_FLAG;
	return is_cp_ptr_utf8(&codepages[cp_index]);
}
