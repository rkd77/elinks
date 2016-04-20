
#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>
#include <sys/stat.h>
#include "util/conv.h"
#include "util/string.h"

/** Data read about an entry in a directory.
 * The strings pointed to by this structure are in the system
 * charset (i.e. LC_CTYPE) and must be freed with mem_free().  */
struct directory_entry {
	/** The various attribute info collected with the @c stat_* functions. */
	unsigned char *attrib;

	/** The full path of the dir entry. */
	unsigned char *name;
};

/** First information such as permissions is gathered for each directory entry.
 * All entries are then sorted. */
struct directory_entry *
get_directory_entries(unsigned char *dirname, int get_hidden_files);

int file_exists(const unsigned char *filename);
int file_can_read(const unsigned char *filename);
int file_is_dir(const unsigned char *filename);

/** Strips all directory stuff from @a filename and returns the
 * position of where the actual filename starts */
unsigned char *get_filename_position(unsigned char *filename);

/** Tilde is only expanded for the current users homedir (~/).
 * The returned file name is allocated. */
unsigned char *expand_tilde(unsigned char *filename);

/*! @brief Generate a unique file name by trial and error based on the
 * @a fileprefix by adding suffix counter (e.g. '.42').
 *
 * The returned file name is allocated if @a fileprefix is not unique. */
unsigned char *get_unique_name(unsigned char *fileprefix);

/** Checks various environment variables to get the name of the temp dir.
 * Returns a filename by concatenating "<tmpdir>/<name>". */
unsigned char *get_tempdir_filename(unsigned char *name);

/** Read a line from @a file into the dynamically allocated @a line,
 * increasing @a line if necessary. Ending whitespace is trimmed.
 * If a line ends with "\" the next line is read too.
 * If @a line is NULL the returned line is allocated and if file
 * reading fails @a line is free()d. */
unsigned char *file_read_line(unsigned char *line, size_t *linesize,
			      FILE *file, int *linenumber);

/** Safe wrapper for mkstemp().
 * It enforces permissions by calling umask(0177), call mkstemp(), then
 * restore previous umask(). */
int safe_mkstemp(unsigned char *template_);

/** Recursively create directories in @a path. The last element in the path is
 * taken to be a filename, and simply ignored */
int mkalldirs(const unsigned char *path);

/* comparison function for qsort() */
int compare_dir_entries(const void *v1, const void *v2);


/** @name The stat_* functions set the various attributes for directory entries.
 * @{ */

static inline void
stat_type(struct string *string, struct stat *stp)
{
	unsigned char c = '?';

	if (stp) {
		if (S_ISDIR(stp->st_mode)) c = 'd';
		else if (S_ISREG(stp->st_mode)) c = '-';
#ifdef S_ISBLK
		else if (S_ISBLK(stp->st_mode)) c = 'b';
#endif
#ifdef S_ISCHR
		else if (S_ISCHR(stp->st_mode)) c = 'c';
#endif
#ifdef S_ISFIFO
		else if (S_ISFIFO(stp->st_mode)) c = 'p';
#endif
#ifdef S_ISLNK
		else if (S_ISLNK(stp->st_mode)) c = 'l';
#endif
#ifdef S_ISSOCK
		else if (S_ISSOCK(stp->st_mode)) c = 's';
#endif
#ifdef S_ISNWK
		else if (S_ISNWK(stp->st_mode)) c = 'n';
#endif
	}

	add_char_to_string(string, c);
}

static inline void
stat_mode(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_RIGHTS
	unsigned char rwx[10] = "---------";

	if (stp) {
		mode_t mode = stp->st_mode;
		unsigned int shift;

		/* Set permissions attributes for user, group and other */
		for (shift = 0; shift <= 6; shift += 3) {
			mode_t m = mode << shift;

			if (m & S_IRUSR) rwx[shift + 0] = 'r';
			if (m & S_IWUSR) rwx[shift + 1] = 'w';
			if (m & S_IXUSR) rwx[shift + 2] = 'x';
		}

#ifdef S_ISUID
		if (mode & S_ISUID)
			rwx[2] = (mode & S_IXUSR) ? 's' : 'S';
#endif
#ifdef S_ISGID
		if (mode & S_ISGID)
			rwx[5] = (mode & S_IXGRP) ? 's' : 'S';
#endif
#ifdef S_ISVTX
		if (mode & S_ISVTX)
			rwx[8] = (mode & S_IXOTH) ? 't' : 'T';
#endif
	}
	add_to_string(string, rwx);
#endif
	add_char_to_string(string, ' ');
}

static inline void
stat_links(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_HARDLINKS
	if (!stp) {
		add_to_string(string, "    ");
	} else {
		unsigned char lnk[64];

		ulongcat(lnk, NULL, stp->st_nlink, 3, ' ');
		add_to_string(string, lnk);
		add_char_to_string(string, ' ');
	}
#endif
}

static inline void
stat_user(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_USERS
	static unsigned char last_user[64];
	static int last_uid = -1;

	if (!stp) {
		add_to_string(string, "         ");
		return;
	}

	if (stp->st_uid != last_uid) {
		struct passwd *pwd = getpwuid(stp->st_uid);

		if (!pwd || !pwd->pw_name)
			/* ulongcat() can't pad from right. */
			snprintf(last_user, 64, "%-8d", (int) stp->st_uid);
		else
			snprintf(last_user, 64, "%-8.8s", pwd->pw_name);

		last_uid = stp->st_uid;
	}

	add_to_string(string, last_user);
	add_char_to_string(string, ' ');
#endif
}

static inline void
stat_group(struct string *string, struct stat *stp)
{
#ifdef FS_UNIX_USERS
	static unsigned char last_group[64];
	static int last_gid = -1;

	if (!stp) {
		add_to_string(string, "         ");
		return;
	}

	if (stp->st_gid != last_gid) {
		struct group *grp = getgrgid(stp->st_gid);

		if (!grp || !grp->gr_name)
			/* ulongcat() can't pad from right. */
			snprintf(last_group, 64, "%-8d", (int) stp->st_gid);
		else
			snprintf(last_group, 64, "%-8.8s", grp->gr_name);

		last_gid = stp->st_gid;
	}

	add_to_string(string, last_group);
	add_char_to_string(string, ' ');
#endif
}

static inline void
stat_size(struct string *string, struct stat *stp)
{
	/* Check if st_size will cause overflow. */
	/* FIXME: See bug 497 for info about support for big files. */
	if (!stp || stp->st_size != (unsigned long) stp->st_size) {
		add_to_string(string, "         ");

	} else {
		unsigned char size[64];
		int width = 9;

		/* First try to fit the file size into 8 digits ... */
		width = ulongcat(size, NULL, stp->st_size, width, ' ');
		if (0 < width && width < sizeof(size)) {
			/* ... if that is not enough expand the size buffer.
			 * This will only break the column alignment of the size
			 * attribute if really needed. */
			ulongcat(size, NULL, stp->st_size, width, ' ');
		}

		add_to_string(string, size);
		add_char_to_string(string, ' ');
	}
}

static inline void
stat_date(struct string *string, struct stat *stp)
{
#ifdef HAVE_STRFTIME
	if (stp) {
		time_t current_time = time(NULL);
		time_t when = stp->st_mtime;
		unsigned char *fmt;

		if (current_time > when + 6L * 30L * 24L * 60L * 60L
		    || current_time < when - 60L * 60L)
			fmt = "%b %e  %Y";
		else
			fmt = "%b %e %H:%M";

		add_date_to_string(string, fmt, &when);
		add_char_to_string(string, ' ');
		return;
	}
#endif
	add_to_string(string, "             ");
}


#endif
