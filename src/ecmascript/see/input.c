/* Input for SEE */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <see/see.h>
#include <string.h>
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
SEE_input_elinks(struct SEE_interpreter *interp, unsigned char *s)
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
SEE_string_to_unsigned_char(struct SEE_string *S)
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
SEE_value_to_unsigned_char(struct SEE_interpreter *interp, struct SEE_value *val)
{
	struct SEE_value result;

	SEE_ToString(interp, val, &result);
	return SEE_string_to_unsigned_char(result.u.string);
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
