
#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>

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
int safe_mkstemp(unsigned char *template);

/** Recursively create directories in @a path. The last element in the path is
 * taken to be a filename, and simply ignored */
int mkalldirs(const unsigned char *path);

#endif
