
#ifndef EL__UTIL_FILE_H
#define EL__UTIL_FILE_H

#include <stdio.h>

struct directory_entry {
	/* The various attribute info collected with the stat_* functions. */
	unsigned char *attrib;

	/* The full path of the dir entry. */
	unsigned char *name;
};

/* First information such as permissions is gathered for each directory entry.
 * All entries are then sorted. */
struct directory_entry *
get_directory_entries(unsigned char *dirname, int get_hidden_files);

int file_exists(const unsigned char *filename);
int file_can_read(const unsigned char *filename);
int file_is_dir(const unsigned char *filename);

/* Strips all directory stuff from @filename and returns the
 * position of where the actual filename starts */
unsigned char *get_filename_position(unsigned char *filename);

/* Tilde is only expanded for the current users homedir (~/). */
/* The returned file name is allocated. */
unsigned char *expand_tilde(unsigned char *filename);

/* Generate a unique file name by trial and error based on the @fileprefix by
 * adding suffix counter (e.g. '.42'). */
/* The returned file name is allocated if @fileprefix is not unique. */
unsigned char *get_unique_name(unsigned char *fileprefix);

/* Checks various environment variables to get the name of the temp dir.
 * Returns a filename by concatenating "<tmpdir>/<name>". */
unsigned char *get_tempdir_filename(unsigned char *name);

/* Read a line from @file into the dynamically allocated @line, increasing
 * @line if necessary. Ending whitespace is trimmed. If a line ends
 * with "\" the next line is read too. */
/* If @line is NULL the returned line is allocated and if file reading fails
 * @line is free()d. */
unsigned char *file_read_line(unsigned char *line, size_t *linesize,
			      FILE *file, int *linenumber);

/* Safe wrapper for mkstemp().
 * It enforces permissions by calling umask(0177), call mkstemp(), then
 * restore previous umask(). */
int safe_mkstemp(unsigned char *template);

/* Create all the necessary directories for PATH (a file). */
int mkalldirs(const unsigned char *path);

#endif
