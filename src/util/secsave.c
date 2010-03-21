/** Secure file saving handling
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h" /* Needed for mkstemp() on win32 */
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* If infofiles.secure_save is set:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * A call to secure_open("/home/me/.elinks/filename", mask) will open a file
 * named "filename.tmp_XXXXXX" in /home/me/.elinks/ and return a pointer to a
 * structure secure_save_info on success or NULL on error.
 *
 * filename.tmp_XXXXXX can't conflict with any file since it's created using
 * mkstemp(). XXXXXX is a random string.
 *
 * Subsequent write operations are done using returned secure_save_info FILE *
 * field named fp.
 *
 * If an error is encountered, secure_save_info int field named err is set
 * (automatically if using secure_fp*() functions or by programmer)
 *
 * When secure_close() is called, "filename.tmp_XXXXXX" is flushed and closed,
 * and if secure_save_info err field has a value of zero, "filename.tmp_XXXXXX"
 * is renamed to "filename". If this succeeded, then secure_close() returns 0.
 *
 * WARNING: since rename() is used, any symlink called "filename" may be
 * replaced by a regular file. If destination file isn't a regular file,
 * then secsave is disabled for that file.
 *
 * If infofiles.secure_save is unset:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * No temporary file is created, "filename" is truncated, all operations are
 * done on it, no rename nor flush occur, symlinks are preserved.
 *
 * In both cases:
 * ~~~~~~~~~~~~~
 *
 * Access rights are affected by secure_open() mask parameter.
 */

/* FIXME: locking system on files about to be rewritten ? */
/* FIXME: Low risk race conditions about ssi->file_name. */

enum secsave_errno secsave_errno = SS_ERR_NONE;


/** Open a file for writing in a secure way. @returns a pointer to a
 * structure secure_save_info on success, or NULL on failure. */
static struct secure_save_info *
secure_open_umask(unsigned char *file_name)
{
	struct stat st;
	struct secure_save_info *ssi;

	secsave_errno = SS_ERR_NONE;

	/* XXX: This is inherently evil and has no place in util/, which
	 * should be independent on such stuff. What do we do, except blaming
	 * Jonas for noticing it? --pasky */
	if ((get_cmd_opt_bool("no-connect")
	     || get_cmd_opt_int("session-ring"))
	    && !get_cmd_opt_bool("touch-files")) {
		secsave_errno = SS_ERR_DISABLED;
		return NULL;
	}

	ssi = mem_calloc(1, sizeof(*ssi));
	if (!ssi) {
		secsave_errno = SS_ERR_OUT_OF_MEM;
		goto end;
	}

	ssi->secure_save = get_opt_bool("infofiles.secure_save", NULL);

	ssi->file_name = stracpy(file_name);
	if (!ssi->file_name) {
		secsave_errno = SS_ERR_OUT_OF_MEM;
		goto free_f;
	}

	/* Check properties of final file. */
#ifdef FS_UNIX_SOFTLINKS
	if (lstat(ssi->file_name, &st)) {
#else
	if (stat(ssi->file_name, &st)) {
#endif
		/* We ignore error caused by file inexistence. */
		if (errno != ENOENT) {
			/* lstat() error. */
			ssi->err = errno;
			secsave_errno = SS_ERR_STAT;
			goto free_file_name;
		}
	} else {
		if (!S_ISREG(st.st_mode)) {
			/* Not a regular file, secure_save is disabled. */
			ssi->secure_save = 0;
		} else {
#ifdef HAVE_ACCESS
			/* XXX: access() do not work with setuid programs. */
			if (access(ssi->file_name, R_OK | W_OK) < 0) {
				ssi->err = errno;
				secsave_errno = SS_ERR_ACCESS;
				goto free_file_name;
			}
#else
			FILE *f1;

			/* We still have a race condition here between
			 * [l]stat() and fopen() */

			f1 = fopen(ssi->file_name, "rb+");
			if (f1) {
				fclose(f1);
			} else {
				ssi->err = errno;
				secsave_errno = SS_ERR_OPEN_READ;
				goto free_file_name;
			}
#endif
		}
	}

	if (ssi->secure_save) {
		/* We use a random name for temporary file, mkstemp() opens
		 * the file and return a file descriptor named fd, which is
		 * then converted to FILE * using fdopen().
		 */
		int fd;
		unsigned char *randname = straconcat(ssi->file_name,
						     ".tmp_XXXXXX",
						     (unsigned char *) NULL);

		if (!randname) {
			secsave_errno = SS_ERR_OUT_OF_MEM;
			goto free_file_name;
		}

		/* No need to use safe_mkstemp() here. --Zas */
		fd = mkstemp(randname);
		if (fd == -1) {
			secsave_errno = SS_ERR_MKSTEMP;
			mem_free(randname);
			goto free_file_name;
		}

		ssi->fp = fdopen(fd, "w");
		if (!ssi->fp) {
			secsave_errno = SS_ERR_OPEN_WRITE;
			ssi->err = errno;
			mem_free(randname);
			goto free_file_name;
		}

		ssi->tmp_file_name = randname;
	} else {
		/* No need to create a temporary file here. */
		ssi->fp = fopen(ssi->file_name, "wb");
		if (!ssi->fp) {
			secsave_errno = SS_ERR_OPEN_WRITE;
			ssi->err = errno;
			goto free_file_name;
		}
	}

	return ssi;

free_file_name:
	mem_free(ssi->file_name);
	ssi->file_name = NULL;

free_f:
	mem_free(ssi);
	ssi = NULL;

end:
	return NULL;
}

/* @relates secure_save_info */
struct secure_save_info *
secure_open(unsigned char *file_name)
{
	struct secure_save_info *ssi;
	mode_t saved_mask;
#ifdef CONFIG_OS_WIN32
	/* There is neither S_IRWXG nor S_IRWXO under crossmingw32-gcc */
	const mode_t mask = 0177;
#else
	const mode_t mask = S_IXUSR | S_IRWXG | S_IRWXO;
#endif

	saved_mask = umask(mask);
	ssi = secure_open_umask(file_name);
	umask(saved_mask);

	return ssi;
}

/** Close a file opened with secure_open(). @returns 0 on success,
 * errno or -1 on failure.
 * @relates secure_save_info */
int
secure_close(struct secure_save_info *ssi)
{
	int ret = -1;

	if (!ssi) return ret;
	if (!ssi->fp) goto free;

	if (ssi->err) {	/* Keep previous errno. */
		ret = ssi->err;
		fclose(ssi->fp); /* Close file */
		goto free;
	}

	/* Ensure data is effectively written to disk, we first flush libc buffers
	 * using fflush(), then fsync() to flush kernel buffers, and finally call
	 * fclose() (which call fflush() again, but the first one is needed since
	 * it doesn't make much sense to flush kernel buffers and then libc buffers,
	 * while closing file releases file descriptor we need to call fsync(). */
#if defined(HAVE_FFLUSH) || defined(HAVE_FSYNC)
	if (ssi->secure_save) {
		int fail = 0;

#ifdef HAVE_FFLUSH
		fail = (fflush(ssi->fp) == EOF);
#endif

#ifdef HAVE_FSYNC
		if (!fail && get_opt_bool("infofiles.secure_save_fsync", NULL))
			fail = fsync(fileno(ssi->fp));
#endif

		if (fail) {
			ret = errno;
			secsave_errno = SS_ERR_OTHER;

			fclose(ssi->fp); /* Close file, ignore errors. */
			goto free;
		}
	}
#endif

	/* Close file. */
	if (fclose(ssi->fp) == EOF) {
		ret = errno;
		secsave_errno = SS_ERR_OTHER;
		goto free;
	}

	if (ssi->secure_save && ssi->file_name && ssi->tmp_file_name) {
#if defined(CONFIG_OS_OS2) || defined(CONFIG_OS_WIN32)
		/* OS/2 needs this, however it breaks atomicity on
		 * UN*X. */
		unlink(ssi->file_name);
#endif
		/* FIXME: Race condition on ssi->file_name. The file
		 * named ssi->file_name may have changed since
		 * secure_open() call (where we stat() file and
		 * more..).  */
		if (rename(ssi->tmp_file_name, ssi->file_name) == -1) {
			ret = errno;
			secsave_errno = SS_ERR_RENAME;
			goto free;
		}
	}

	ret = 0;	/* Success. */

free:
	mem_free_if(ssi->tmp_file_name);
	mem_free_if(ssi->file_name);
	mem_free_if(ssi);

	return ret;
}


/** fputs() wrapper, set ssi->err to errno on error. If ssi->err is set when
 * called, it immediatly returns EOF.
 * @relates secure_save_info */
int
secure_fputs(struct secure_save_info *ssi, const char *s)
{
	int ret;

	if (!ssi || !ssi->fp || ssi->err) return EOF;

	ret = fputs(s, ssi->fp);
	if (ret == EOF) {
		secsave_errno = SS_ERR_OTHER;
		ssi->err = errno;
	}

	return ret;
}


/** fputc() wrapper, set ssi->err to errno on error. If ssi->err is set when
 * called, it immediatly returns EOF.
 * @relates secure_save_info */
int
secure_fputc(struct secure_save_info *ssi, int c)
{
	int ret;

	if (!ssi || !ssi->fp || ssi->err) return EOF;

	ret = fputc(c, ssi->fp);
	if (ret == EOF) {
		ssi->err = errno;
		secsave_errno = SS_ERR_OTHER;
	}

	return ret;
}

/** fprintf() wrapper, set ssi->err to errno on error and return a negative
 * value. If ssi->err is set when called, it immediatly returns -1.
 * @relates secure_save_info */
int
secure_fprintf(struct secure_save_info *ssi, const char *format, ...)
{
	va_list ap;
	int ret;

	if (!ssi || !ssi->fp || ssi->err) return -1;

	va_start(ap, format);
	ret = vfprintf(ssi->fp, format, ap);
	if (ret < 0) ssi->err = errno;
	va_end(ap);

	return ret;
}

unsigned char *
secsave_strerror(enum secsave_errno secsave_error, struct terminal *term)
{
	switch (secsave_error) {
	case SS_ERR_OPEN_READ:
		return _("Cannot read the file", term);
	case SS_ERR_STAT:
		return _("Cannot get file status", term);
	case SS_ERR_ACCESS:
		return _("Cannot access the file", term);
	case SS_ERR_MKSTEMP:
		return _("Cannot create temp file", term);
	case SS_ERR_RENAME:
		return _("Cannot rename the file", term);
	case SS_ERR_DISABLED:
		return _("File saving disabled by option", term);
	case SS_ERR_OUT_OF_MEM:
		return _("Out of memory", term);
	case SS_ERR_OPEN_WRITE:
		return _("Cannot write the file", term);
	case SS_ERR_NONE: /* Impossible. */
	case SS_ERR_OTHER:
	default:
		return _("Secure file saving error", term);
	}
}
