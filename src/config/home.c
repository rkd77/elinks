/* Get home directory */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/home.h"
#include "config/options.h"
#include "intl/libintl.h"
#include "main/main.h"
#include "osdep/osdep.h"
#include "util/memory.h"
#include "util/string.h"


int first_use = 0;
static char *xdg_config_home = NULL;

static inline void
strip_trailing_dir_sep(char *path)
{
	int i;

	for (i = strlen(path) - 1; i > 0; i--)
		if (!dir_sep(path[i]))
			break;

	path[i + 1] = 0;
}

#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
  /* Win32, OS/2, DOS */
#define HAS_DEVICE(P) (isasciialpha((P)[0]) && (P)[1] == ':')
#define IS_ABSOLUTE_PATH(P) (dir_sep((P)[0]) || HAS_DEVICE (P))
#define IS_PATH_WITH_DIR(P) (strchr ((const char *)(P), '/') || strchr ((const char *)(P), '\\') || HAS_DEVICE (P))
#else
  /* Unix */
#define IS_ABSOLUTE_PATH(P) dir_sep((P)[0])
#define IS_PATH_WITH_DIR(P) strchr ((const char *)(P), '/')
#endif

static char *
path_skip_root(const char *file_name)
{
#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
	/* Skip \\server\share or //server/share */

	if (dir_sep(file_name[0]) && dir_sep(file_name[1]) && file_name[2] && !dir_sep(file_name[2])) {
		char *p = strchr (file_name + 2, CHAR_DIR_SEP);

#ifdef CONFIG_WIN32
		{
			char *q = strchr (file_name + 2, '/');

			if (p == NULL || (q != NULL && q < p)) {
				p = q;
			}
		}
#endif
		if (p && p > file_name + 2 && p[1]) {
			file_name = p + 1;

			while (file_name[0] && !dir_sep(file_name[0])) {
				file_name++;
			}
			/* Possibly skip a backslash after the share name */

			if (dir_sep(file_name[0])) {
				file_name++;
			}
			return (char *)file_name;
		}
	}
#endif

	/* Skip initial slashes */
	if (dir_sep(file_name[0])) {
		while (dir_sep(file_name[0])) {
			file_name++;
		}
		return (char *)file_name;
	}

#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
	/* Skip X:\ */

	if (HAS_DEVICE(file_name) && dir_sep(file_name[2])) {
		return (char *)file_name + 3;
	}
#endif
	return NULL;
}

static int
mkdir_with_parents(const char *pathname, mode_t mode)
{
	char *fn = stracpy(pathname);
	char *p;

	if (!fn) {
		return -1;
	}

	if (IS_ABSOLUTE_PATH(fn)) {
		p = path_skip_root(fn);
	} else {
		p = fn;
	}

	do {
		struct stat st;
		while (*p && !dir_sep(*p)) {
			p++;
		}

		if (!*p) {
			p = NULL;
		} else {
			*p = '\0';
		}

		if (stat(fn, &st)) {
			if (mkdir(fn, mode) == -1 && errno != EEXIST) {
				int errno_save = errno;

				if (errno != ENOENT || !p) {
					mem_free(fn);
					errno = errno_save;
					return -1;
				}
			}
		} else {
			if (!S_ISDIR(st.st_mode)) {
				mem_free(fn);
				errno = ENOTDIR;
				return -1;
			}
		}

		if (p) {
			*p++ = CHAR_DIR_SEP;

			while (*p && dir_sep(*p)) {
				p++;
			}
		}
	} while (p);
	mem_free(fn);
	return 0;
}

static char *
test_confdir(const char *home, const char *path,
	     char *error_message)
{
	struct stat st;
	char *confdir;

	if (!path || !*path) return NULL;

	if (home && *home && !dir_sep(*path))
		confdir = straconcat(home, STRING_DIR_SEP, path,
				     (char *) NULL);
	else
		confdir = stracpy(path);

	if (!confdir) return NULL;

	strip_trailing_dir_sep(confdir);

	if (stat(confdir, &st)) {
		if (!mkdir(confdir, 0700)) {
#if 0
		/* I've no idea if following is needed for newly created
		 * directories.  It's bad thing to do it everytime. --pasky */
#ifdef HAVE_CHMOD
			chmod(home_elinks, 0700);
#endif
#endif
			return confdir;
		}
		if (!mkdir_with_parents(confdir, 0700)) {
			return confdir;
		}
	} else if (S_ISDIR(st.st_mode)) {
		first_use = 0;
		return confdir;
	}

	if (error_message) {
		usrerror(gettext(error_message), path, confdir);
		sleep(3);
	}

	mem_free(confdir);

	return NULL;
}

char *
get_xdg_config_home(void)
{
	return xdg_config_home;
}

static char *
get_xdg_config_home_internal(void)
{
	char *config_dir = NULL;
	char *elinks_confdir;
	char *pa;
	char *g_xdg_config_home;
	char *home;

	if (xdg_config_home) {
		return xdg_config_home;
	}
	elinks_confdir = getenv("ELINKS_CONFDIR");
	pa = get_cmd_opt_str("config-dir");

	if (elinks_confdir && *elinks_confdir && (!pa || !*pa)) {
		xdg_config_home = test_confdir(NULL, elinks_confdir, NULL);

		if (xdg_config_home) goto end;
	}
	g_xdg_config_home = getenv("XDG_CONFIG_HOME");

	if (g_xdg_config_home && *g_xdg_config_home) {
		xdg_config_home = test_confdir(g_xdg_config_home,
						pa,
						N_("Commandline options -config-dir set to %s, "
						"but could not create directory %s."));
		if (xdg_config_home) {
			goto end;
		}
		xdg_config_home = test_confdir(g_xdg_config_home, "elinks", NULL);

		if (xdg_config_home) {
			goto end;
		}

		return NULL;
	}
	home = getenv("HOME");

	if (!home || !*home) {
		return NULL;
	}
	config_dir = straconcat(home, STRING_DIR_SEP, ".config", NULL);

	if (!config_dir) {
		return NULL;
	}
	xdg_config_home = test_confdir(config_dir,
				pa,
				N_("Commandline options -config-dir set to %s, "
				"but could not create directory %s."));
	if (xdg_config_home) {
		goto end;
	}
	xdg_config_home = test_confdir(config_dir, "elinks", NULL);

end:
	mem_free_if(config_dir);

	if (xdg_config_home) {
		add_to_strn(&xdg_config_home, STRING_DIR_SEP);
		return xdg_config_home;
	}

	return NULL;
}


#if 0
/* TODO: Check possibility to use <libgen.h> dirname. */
static char *
elinks_dirname(char *path)
{
	int i;
	char *dir;

	if (!path) return NULL;

	dir = stracpy(path);
	if (!dir) return NULL;

	for (i = strlen(dir) - 1; i >= 0; i--)
		if (dir_sep(dir[i]))
			break;

	dir[i + 1] = 0;

	return dir;
}

static char *
get_home(void)
{
	char *home_elinks;
	char *envhome = getenv("HOME_ETC") ?: getenv("HOME");
	char *home = NULL;

	if (!home && envhome)
		home = stracpy(envhome);
	if (!home)
		home = user_appdata_directory();
	if (!home)
		home = elinks_dirname(program.path);

	if (home)
		strip_trailing_dir_sep(home);

	home_elinks = test_confdir(home,
				   get_cmd_opt_str("config-dir"),
				   N_("Commandline options -config-dir set to %s, "
				      "but could not create directory %s."));
	if (home_elinks) goto end;

	home_elinks = test_confdir(home, getenv("ELINKS_CONFDIR"),
				   N_("ELINKS_CONFDIR set to %s, "
				      "but could not create directory %s."));
	if (home_elinks) goto end;

	home_elinks = test_confdir(home, ".elinks", NULL);
	if (home_elinks) goto end;

	home_elinks = test_confdir(home, "elinks", NULL);

end:
	if (home_elinks)
		add_to_strn(&home_elinks, STRING_DIR_SEP);
	mem_free_if(home);

	return home_elinks;
}
#endif

void
init_home(void)
{
	first_use = 1;
	xdg_config_home = get_xdg_config_home_internal();
	if (!xdg_config_home) {
		ERROR(gettext("Unable to find or create ELinks config "
		      "directory. Please check if you have $HOME "
		      "variable set correctly and if you have "
		      "write permission to your home directory."));
		sleep(3);
		return;
	}
}

void
done_home(void)
{
	mem_free_set(&xdg_config_home, NULL);
}
