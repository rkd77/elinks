#ifndef EL__INTL_CHARSETS_H
#define EL__INTL_CHARSETS_H

/* The TRE check in configure.in assumes unicode_val_T is uint32_t.  */
typedef uint32_t unicode_val_T;

/* U+0020 SPACE.  Normally the same as ' ' or L' ' but perhaps ELinks
 * shouldn't rely on that.  */
#define UCS_SPACE ((unicode_val_T) 0x0020)

/* U+00A0 NO-BREAK SPACE.  */
#define UCS_NO_BREAK_SPACE ((unicode_val_T) 0x00A0)

/* U+00AD SOFT HYPHEN.  */
#define UCS_SOFT_HYPHEN ((unicode_val_T) 0x00AD)

/* U+FFFD REPLACEMENT CHARACTER.  Used when no Unicode mapping is
 * known for a byte in a codepage, or when invalid UTF-8 is received
 * from a terminal.  After generating the character, ELinks then
 * treats it like any other Unicode character.  The user can also type
 * this character directly, and it can occur in documents.  */
#define UCS_REPLACEMENT_CHARACTER ((unicode_val_T) 0xFFFD)

/* A special value that fits in unicode_val_T but is outside the range
 * of Unicode characters.  utf8_to_unicode and cp_to_unicode return
 * this if the input is too short.  This is also used as a placeholder
 * for the second cell of a double-cell character.  */
#define UCS_NO_CHAR ((unicode_val_T) 0xFFFFFFFD)

/* If ELinks should display a double-cell character but there is only
 * one cell available, it displays this character instead.  This must
 * be a single-cell character but need not be unique.  Possible values
 * might be U+0020 SPACE or U+303F IDEOGRAPHIC HALF FILL SPACE.
 *
 * Some BFU widgets (at least input fields and list boxes) currently
 * ignore this setting and use U+0020 instead.  (They first draw spaces
 * and then overwrite with text; look for utf8_cells2bytes calls.)
 * We should fix that if we ever change the value.  */
#define UCS_ORPHAN_CELL ((unicode_val_T) 0x20)

/* &nbsp; replacement character. See u2cp().
 * UTF-8 strings should use the encoding of U+00A0 instead. */
#define NBSP_CHAR ((unsigned char) 1)
#define NBSP_CHAR_STRING "\001"

/* How to convert a byte from a source charset.  This is used in an
 * array (struct conv_table[256]) indexed by the byte value.  */
struct conv_table {
	/* 0 if this is the final byte of a character, or 1 if more
	 * bytes are needed.  */
	int t;
	union {
		/* If @t==0: a null-terminated string that is the
		 * corresponding character in the target charset.
		 * Normally, the string is statically allocated.
		 * However, if the conversion table is to UTF-8, then
		 * the strings in elements 0x80 to 0xFF are allocated
		 * with @mem_alloc and owned by the table.  */
		const unsigned char *str;
		/* If @t==1: a pointer to a nested conversion table
		 * (with 256 elements) that describes how to convert
		 * each possible subsequent byte.  The conversion
		 * table owns the nested conversion table.  */
		struct conv_table *tbl;
	} u;
};

enum convert_string_mode {
	CSM_DEFAULT, /* Convert any char. */
	CSM_QUERY, /* Special handling of '&' and '=' chars. */
	CSM_FORM, /* Special handling of '&' and '=' chars in forms. */
	CSM_NONE, /* Convert nothing. */
};

/* How to translate U+00A0 NO-BREAK SPACE.  If u2cp_ is converting to
 * UTF-8, it ignores this choice and just encodes the U+00A0.  */
enum nbsp_mode {
	/* Convert to NBSP_CHAR.  This lets the HTML renderer
	 * recognize nbsp even if the codepage doesn't support
	 * nbsp.  (VISCII doesn't.)  */
	NBSP_MODE_HACK = 0,

	/* Convert to normal ASCII space.  */
	NBSP_MODE_ASCII = 1
};

struct conv_table *get_translation_table(int, int);
const unsigned char *get_entity_string(const unsigned char *str,
				       const int strlen, int encoding);

/* The convert_string() name is also used by Samba (version 3.0.3), which
 * provides libnss_wins.so.2, which is called somewhere inside
 * _nss_wins_gethostbyname_r(). This name clash causes the elinks hostname
 * lookup thread to crash so we need to rename the symbol. */
/* Reported by Derek Poon and filed as bug 453 */
#undef convert_string
#define convert_string convert_string_elinks

/* This routine converts a string from one charset to another according to the
 * passed @convert_table, potentially also decoding SGML (HTML) entities along
 * the way (according to @mode). It either returns dynamically allocated
 * converted string of length @length, or if the @callback is non-NULL it calls
 * it each few bytes instead and always returns NULL (@length is undefined).
 * Note that it's ok not to care and pass NULL as @length. */
unsigned char *convert_string(struct conv_table *convert_table,
			      unsigned char *chars, int charslen, int cp,
			      enum convert_string_mode mode, int *length,
			      void (*callback)(void *data, unsigned char *buf, int buflen),
			      void *callback_data);

int get_cp_index(const unsigned char *);
unsigned char *get_cp_name(int);
unsigned char *get_cp_config_name(int);
unsigned char *get_cp_mime_name(int);
int is_cp_utf8(int);
void free_conv_table(void);
unsigned char *encode_utf8(unicode_val_T);
#ifdef CONFIG_UTF8
unsigned char *utf8_prevchar(unsigned char *, int, unsigned char *);
int utf8charlen(const unsigned char *);
int utf8_char2cells(unsigned char *, unsigned char *);
int utf8_ptr2cells(unsigned char *, unsigned char *);
int utf8_ptr2chars(unsigned char *, unsigned char *);
int utf8_cells2bytes(unsigned char *, int, unsigned char *);
/* How utf8_step_forward and utf8_step_backward count steps.  */
enum utf8_step {
	/* Each step is one character, even if it is a combining or
	 * double-width character.  */
	UTF8_STEP_CHARACTERS,

	/* Each step is one cell.  If the specified number of steps
	 * would end in the middle of a double-width character, do not
	 * include the character.  */
	UTF8_STEP_CELLS_FEWER,

	/* Each step is one cell.  If the specified number of steps
	 * would end in the middle of a double-width character,
	 * include the whole character.  */
	UTF8_STEP_CELLS_MORE
};
unsigned char *utf8_step_forward(unsigned char *, unsigned char *,
				 int, enum utf8_step, int *);
unsigned char *utf8_step_backward(unsigned char *, unsigned char *,
				  int, enum utf8_step, int *);
int unicode_to_cell(unicode_val_T);
unicode_val_T unicode_fold_label_case(unicode_val_T);
int strlen_utf8(unsigned char **);
#endif /* CONFIG_UTF8 */
unicode_val_T utf8_to_unicode(unsigned char **, const unsigned char *);
unicode_val_T cp_to_unicode(int, unsigned char **, const unsigned char *);

unicode_val_T cp2u(int, unsigned char);
const unsigned char *cp2utf8(int, int);

const unsigned char *u2cp_(unicode_val_T, int, enum nbsp_mode);
#define u2cp(u, to) u2cp_(u, to, NBSP_MODE_HACK)
#define u2cp_no_nbsp(u, to) u2cp_(u, to, NBSP_MODE_ASCII)

void init_charsets_lookup(void);
void free_charsets_lookup(void);

/* UTF-16 encodes each Unicode character U+0000...U+FFFF as a single
 * 16-bit code unit, and each character U+10000...U+10FFFF as a pair
 * of two code units: a high surrogate followed by a low surrogate.
 * The range U+D800...U+DFFF is reserved for these surrogates.  */
#define is_utf16_surrogate(u)           (((u) & 0xFFFFF800) == 0xD800)
#define is_utf16_high_surrogate(u)      (((u) & 0xFFFFFC00) == 0xD800)
#define is_utf16_low_surrogate(u)       (((u) & 0xFFFFFC00) == 0xDC00)
#define join_utf16_surrogates(high,low) (0x10000 + (((high) - 0xD800L) << 10) + ((low) - 0xDC00))
#define needs_utf16_surrogates(u)       ((uint32_t) ((u) - 0x10000) < 0x100000)
#define get_utf16_high_surrogate(u)     (0xD800 + (((u) - 0x10000) >> 10))
#define get_utf16_low_surrogate(u)      (0xDC00 + ((u) & 0x3FF))

#endif
