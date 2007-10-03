
#ifndef EL__SETUP_H
#define EL__SETUP_H

#define VERSION_STRING			VERSION

#define COPYRIGHT_STRING		"(C) 1999 - 2002 Mikulas Patocka\n" \
			 		"(C) 2001 - 2004 Petr Baudis\n"	\
			 		"(C) 2002 - 2007 Jonas Fonseca\n"

/* This option will take effect when WWW_HOME environment variable is NOT
 * set - you'll go automatically to this URL. If the value is just "",
 * you'll get either goto dialog or empty page, depending on the value of
 * startup_goto_dialog. */
#define WWW_HOME_URL			""

#define ELINKS_WEBSITE_URL		"http://elinks.cz/"
#define ELINKS_AUTHORS_URL		"http://elinks.cz/authors.html"
#define ELINKS_DOC_URL			"http://elinks.cz/documentation/"
#define ELINKS_BUGS_URL			"http://bugzilla.elinks.cz/"
#define ELINKS_GITWEB_URL		"http://repo.or.cz/w/elinks.git"

#define ELINKS_SOCK_NAME		"socket"
#define ELINKS_PORT			23456
#define ELINKS_TEMPNAME_PREFIX		"elinks"

#define DNS_CACHE_TIMEOUT		3600	/* in seconds */

#define HTTP_KEEPALIVE_TIMEOUT		60000
#define FTP_KEEPALIVE_TIMEOUT		600000
#define NNTP_KEEPALIVE_TIMEOUT		600000
#define MAX_KEEPALIVE_CONNECTIONS	30
#define KEEPALIVE_CHECK_TIME		((milliseconds_T) 20000)

#define MAX_REDIRECTS			10

#define MEMORY_CACHE_GC_PERCENT		90
#define MAX_CACHED_OBJECT_PERCENT	25

#define MAX_INPUT_HISTORY_ENTRIES	256

#define SCROLL_ITEMS			2

#define DIALOG_LEFT_BORDER		3
#define DIALOG_TOP_BORDER		1
#define DIALOG_LEFT_INNER_BORDER	2
#define DIALOG_TOP_INNER_BORDER		0
#define DIALOG_FRAME			2
#define DIALOG_MIN_WIDTH		42

#define DIALOG_LB			(DIALOG_LEFT_BORDER + DIALOG_LEFT_INNER_BORDER + 1)
#define DIALOG_TB			(DIALOG_TOP_BORDER + DIALOG_TOP_INNER_BORDER + 1)

#define ESC_TIMEOUT			((milliseconds_T) 200)

#define DISPLAY_TIME_MIN		((milliseconds_T) 200)
#define DISPLAY_TIME			20

#define HTML_LEFT_MARGIN		3
#define HTML_MAX_TABLE_LEVEL		10
#define HTML_MAX_FRAME_DEPTH		5
#define HTML_CHAR_WIDTH			7
#define HTML_CHAR_HEIGHT		12
#define HTML_FRAME_CHAR_WIDTH		10
#define HTML_FRAME_CHAR_HEIGHT		16
#define HTML_TABLE_2ND_PASS
#define HTML_DEFAULT_INPUT_SIZE		20
#define HTML_MAX_COLSPAN		32768
#define HTML_MAX_ROWSPAN		32768
#define HTML_MAX_CELLS_MEMORY		32*1024*1024

#define MAX_STR_LEN			1024

#define RESOURCE_INFO_REFRESH		((milliseconds_T) 100)

#define DOWN_DLG_MIN			20

#define AUTH_USER_MAXLEN		40 /* enough? */
#define AUTH_PASSWORD_MAXLEN		40

/* Maximum delay for a refresh in seconds. */
#define HTTP_REFRESH_MAX_DELAY		2*24*60*60	/* 2 days */

/* Default mime settings */
#define DEFAULT_MIME_TYPE		"application/octet-stream"
#define DEFAULT_PAGER_PATH		"/usr/bin/pager"
#define DEFAULT_LESS_PATH		"/usr/bin/less"
#define DEFAULT_MORE_PATH		"/usr/bin/more"
#define DEFAULT_MAILCAP_PATH		"~/.mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap:/usr/share/mailcap:/usr/share/misc/mailcap"
#define DEFAULT_MIMETYPES_PATH		"~/.mime.types:/etc/mime.types:/usr/etc/mime.types:/usr/local/etc/mime.types:/usr/share/mime.types:/usr/share/misc/mime.types"

/* Default external commands (see osdep/newwin.c and/or system-specific osdep/
 * files) */
#define DEFAULT_TWTERM_CMD		"twterm -e"
#define DEFAULT_XTERM_CMD		"xterm -e"
#define DEFAULT_SCREEN_CMD		"screen"
#define DEFAULT_OS2_WINDOW_CMD		"cmd /c start /c /f /win"
#define DEFAULT_OS2_FULLSCREEN_CMD	"cmd /c start /c /f /fs"
#define DEFAULT_BEOS_TERM_CMD		"Terminal"

/* Default external programs for protocol.user.* autocreated options */
#define DEFAULT_AC_OPT_MAILTO		"mutt %h -s \"%s\" -i \"%f\""
#define DEFAULT_AC_OPT_TELNET		"telnet %h %p"
#define DEFAULT_AC_OPT_TN3270		"tn3270 %h %p"
#define DEFAULT_AC_OPT_GOPHER		"lynx %u"
#define DEFAULT_AC_OPT_NEWS		"lynx %u"
#define DEFAULT_AC_OPT_IRC		"irc %u"

/* Default terminal size. Used for -dump and for normal runs if detection
 * through ioctl() and environment variables fails. */
#define DEFAULT_TERMINAL_WIDTH		80
#define DEFAULT_TERMINAL_HEIGHT		25

/* If this is non-negative, lines in extra-wide table cells will be wrapped
 * to fit in the screen, with this much extra space.  Try 4. */
#define TABLE_LINE_PADDING		-1

#endif
