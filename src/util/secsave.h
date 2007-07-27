/** Secure file saving handling
 * @file */

#ifndef EL__UTIL_SECSAVE_H
#define EL__UTIL_SECSAVE_H

#include <stdio.h>
#include <sys/types.h> /* mode_t */

struct terminal;

enum secsave_errno {
	SS_ERR_NONE = 0,
	SS_ERR_DISABLED, /**< secsave is disabled. */
	SS_ERR_OUT_OF_MEM, /**< memory allocation failure */

	/* see err field in struct secure_save_info */
	SS_ERR_OPEN_READ,
	SS_ERR_OPEN_WRITE,
	SS_ERR_STAT,
	SS_ERR_ACCESS,
	SS_ERR_MKSTEMP,
	SS_ERR_RENAME,
	SS_ERR_OTHER,
};

extern enum secsave_errno secsave_errno; /**< internal secsave error number */

struct secure_save_info {
	FILE *fp; /**< file stream pointer */
	unsigned char *file_name; /**< final file name */
	unsigned char *tmp_file_name; /**< temporary file name */
	int err; /**< set to non-zero value in case of error */
	int secure_save; /**< use secure save for this file */
};

struct secure_save_info *secure_open(unsigned char *);

int secure_close(struct secure_save_info *);

int secure_fputs(struct secure_save_info *, const char *);
int secure_fputc(struct secure_save_info *, int);

int secure_fprintf(struct secure_save_info *, const char *, ...);

unsigned char *secsave_strerror(enum secsave_errno, struct terminal *);

#endif
