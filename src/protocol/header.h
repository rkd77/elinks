#ifndef EL__PROTOCOL_HEADER_H
#define EL__PROTOCOL_HEADER_H

enum parse_header_param {
	HEADER_PARAM_FOUND,
	HEADER_PARAM_NOT_FOUND,
	HEADER_PARAM_OUT_OF_MEMORY
	/* Might add HEADER_PARAM_SYNTAX_ERROR in a later version.
	 * Unknown values should be treated as errors. */
};

unsigned char *parse_header(unsigned char *, const unsigned char *, unsigned char **);
enum parse_header_param parse_header_param(unsigned char *, unsigned char *, unsigned char **);
unsigned char *get_header_param(unsigned char *, unsigned char *);

#endif
