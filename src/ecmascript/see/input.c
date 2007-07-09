/* Input for SEE */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <see/see.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/see/input.h"
#include "util/memory.h"

static SEE_unicode_t input_elinks_next(struct SEE_input *);
static void input_elinks_close(struct SEE_input *);

static struct SEE_inputclass input_elinks_class = {
	input_elinks_next,
	input_elinks_close
};

struct input_elinks {
	struct SEE_input inp;
	unsigned char *s;
};

static SEE_unicode_t
input_elinks_next(struct SEE_input *inp)
{
	struct input_elinks *input = (struct input_elinks*)inp;
	SEE_unicode_t next;

	next = input->inp.lookahead;

	if (*input->s == '\0') {
		input->inp.eof = 1;
	} else {
		input->inp.lookahead = *input->s++;
		input->inp.eof = 0;
	}
	return next;
}

static void
input_elinks_close(struct SEE_input *inp)
{
	/* nothing */
}

struct SEE_input *
see_input_elinks(struct SEE_interpreter *interp, unsigned char *s)
{
	struct input_elinks *input;

	input = SEE_NEW(interp, struct input_elinks);
	input->inp.interpreter = interp;
	input->inp.inputclass = &input_elinks_class;
	input->inp.filename = NULL;
	input->inp.first_lineno = 1;
	input->s = s;
	SEE_INPUT_NEXT((struct SEE_input *)input);	/* prime */
	return (struct SEE_input *)input;
}

unsigned char *
see_string_to_unsigned_char(struct SEE_string *S)
{
	int i;
	unsigned char *str = mem_alloc(S->length + 1);

	if (!str)
		return NULL;

	for (i = 0; i < S->length; i++)
		str[i] = (unsigned char)S->data[i];
	str[S->length] = '\0';
	return str;
}

unsigned char *
see_value_to_unsigned_char(struct SEE_interpreter *interp, struct SEE_value *val)
{
	struct SEE_value result;

	SEE_ToString(interp, val, &result);
	return see_string_to_unsigned_char(result.u.string);
}

struct SEE_string *
string_to_SEE_string(struct SEE_interpreter *interp, unsigned char *s)
{
	unsigned int len;
	unsigned int i;
	struct SEE_string *str;

	len = s ? strlen(s) : 0;
	str = SEE_string_new(interp, len);
	str->length = len;
	for (i = 0; i < len; i++)
		str->data[i] = s[i];
	return str;
}

void
append_unicode_to_SEE_string(struct SEE_interpreter *interp,
			     struct SEE_string *str,
			     unicode_val_T u)
{
	/* This is supposed to make a string from which
	 * SEE_string_to_unicode() can get the original @u back.
	 * If @u is a surrogate, then that is not possible, so
	 * throw an error instead.  */
	if (u <= 0xFFFF && !is_utf16_surrogate(u)) {
		SEE_string_addch(str, u);
	} else if (needs_utf16_surrogates(u)) {
		SEE_string_addch(str, get_utf16_high_surrogate(u));
		SEE_string_addch(str, get_utf16_low_surrogate(u));
	} else {
		/* str->interpreter exists but is not documented, so don't
		 * use it; use a separate @interp parameter instead.
		 * Also, SEE does not support "%lX".  */
		SEE_error_throw(interp, interp->RangeError,
				"UTF-16 cannot encode U+%.4X",
				(unsigned int) u);
	}
}

unicode_val_T
see_string_to_unicode(struct SEE_interpreter *interp, struct SEE_string *S)
{
	/* This implementation ignores extra characters in the string.  */
	if (S->length < 1) {
		SEE_error_throw(interp, interp->Error,
				"String is empty");
	} else if (!is_utf16_surrogate(S->data[0])) {
		return S->data[0];
	} else if (S->length >= 2
		   && is_utf16_high_surrogate(S->data[0])
		   && is_utf16_low_surrogate(S->data[1])) {
		return join_utf16_surrogates(S->data[0], S->data[1]);
	} else {
		SEE_error_throw(interp, interp->Error,
				"Invalid UTF-16 sequence");
	}
}
