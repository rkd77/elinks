/* FTP directory parsing */

#ifndef EL__PROTOCOL_FTP_PARSE_H
#define EL__PROTOCOL_FTP_PARSE_H

#include "util/string.h"

/* File types. */
/* The value is the char value used when displaying the file type. */
enum ftp_file_type {
	FTP_FILE_PLAINFILE	= '-',
	FTP_FILE_DIRECTORY	= 'd',
	FTP_FILE_SYMLINK	= 'l',
	FTP_FILE_UNKNOWN	= '?',
};

/* Information about one file in a directory listing. */
struct ftp_file_info {
	enum ftp_file_type type;	/* File type */
	struct string name;		/* File name */
	struct string symlink;		/* Link to which file points */
	off_t size;			/* File size. -1 if unknown. */
	time_t mtime;			/* Modification time */
	unsigned int local_time_zone:1;	/* What format the mtime is in */
	mode_t permissions;		/* File permissions */
};

#define FTP_SIZE_UNKNOWN -1

/* File info initializers: */

#define INIT_FTP_FILE_INFO \
	{ FTP_FILE_UNKNOWN, INIT_STRING("", 0), INIT_STRING("", 0), FTP_SIZE_UNKNOWN, 0, 0, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH }

#define INIT_FTP_FILE_INFO_ROOT \
	{ FTP_FILE_DIRECTORY, INIT_STRING("..", 2), INIT_STRING("", 0), FTP_SIZE_UNKNOWN, 0, 0, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH }

struct ftp_file_info *
parse_ftp_file_info(struct ftp_file_info *info, unsigned char *src, int len);

#endif
