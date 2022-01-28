/** Secure file saving handling
 * @file */

#ifndef EL__UTIL_SECSAVE_H
#define EL__UTIL_SECSAVE_H

#include <stdio.h>
#include <sys/types.h> /* mode_t */

#ifdef __cplusplus
extern "C" {
#endif

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

typedef int secsave_errno_T;

extern secsave_errno_T secsave_errno; /**< internal secsave error number */

struct secure_save_info {
	FILE *fp; /**< file stream pointer */
	char *file_name; /**< final file name */
	char *tmp_file_name; /**< temporary file name */
	int err; /**< set to non-zero value in case of error */
	int secure_save; /**< use secure save for this file */
};

struct secure_save_info *secure_open(char *);

int secure_close(struct secure_save_info *);

int secure_fputs(struct secure_save_info *, const char *);
int secure_fputc(struct secure_save_info *, int);

int secure_fprintf(struct secure_save_info *, const char *, ...);

char *secsave_strerror(secsave_errno_T, struct terminal *);

#ifdef __cplusplus
}
#endif

#endif
