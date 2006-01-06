/* Features which vary with the OS */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_IO_H
#include <io.h> /* For win32 && set_bin(). */
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>	/* Need to be after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

/* This is for some exotic TOS mangling when handling passive FTP sockets. */
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#else
#ifdef HAVE_NETINET_IN_SYSTEM_H
#include <netinet/in_system.h>
#endif
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LOCALE_H
/* For the sake of SunOS, keep this away from files including
 * intl/gettext/libintl.h because <locale.h> includes system <libintl.h> which
 * either includes system gettext header or contains gettext function
 * declarations. */
#include <locale.h>
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif


#include "elinks.h"

#include "main/select.h"
#include "osdep/osdep.h"
#include "osdep/signals.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Set a file descriptor to non-blocking mode. It returns a non-zero value
 * on error. */
int
set_nonblocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags | O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 1;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

/* Set a file descriptor to blocking mode. It returns a non-zero value on
 * error. */
int
set_blocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags & ~O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 0;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

void
set_ip_tos_throughput(int socket)
{
#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
	int on = IPTOS_THROUGHPUT;

	setsockopt(socket, IPPROTO_IP, IP_TOS, (char *) &on, sizeof(on));
#endif
}

int
get_e(unsigned char *env)
{
	char *v = getenv(env);

	return (v ? atoi(v) : 0);
}

unsigned char *
get_cwd(void)
{
	int bufsize = 128;
	unsigned char *buf;

	while (1) {
		buf = mem_alloc(bufsize);
		if (!buf) return NULL;
		if (getcwd(buf, bufsize)) return buf;
		mem_free(buf);

		if (errno == EINTR) continue;
		if (errno != ERANGE) return NULL;
		bufsize += 128;
	}

	return NULL;
}

void
set_cwd(unsigned char *path)
{
	if (path) while (chdir(path) && errno == EINTR);
}

unsigned char *
get_shell(void)
{
	unsigned char *shell = GETSHELL;

	if (!shell || !*shell)
		shell = DEFAULT_SHELL;

	return shell;
}


/* Terminal size */

#if !defined(CONFIG_OS2) && !defined(CONFIG_WIN32)

static void
sigwinch(void *s)
{
	((void (*)(void)) s)();
}

void
handle_terminal_resize(int fd, void (*fn)(void))
{
	install_signal_handler(SIGWINCH, sigwinch, fn, 0);
}

void
unhandle_terminal_resize(int fd)
{
	install_signal_handler(SIGWINCH, NULL, NULL, 0);
}

void
get_terminal_size(int fd, int *x, int *y)
{
	struct winsize ws;

	if (ioctl(1, TIOCGWINSZ, &ws) != -1) {
		*x = ws.ws_col;
		*y = ws.ws_row;
	} else {
		*x = 0;
		*y = 0;
	}

	if (!*x) {
		*x = get_e("COLUMNS");
		if (!*x) *x = DEFAULT_TERMINAL_WIDTH;
	}
	if (!*y) {
		*y = get_e("LINES");
		if (!*y) *y = DEFAULT_TERMINAL_HEIGHT;
	}
}

#endif

/* Pipe */

#if defined(CONFIG_UNIX) || defined(CONFIG_BEOS) || defined(CONFIG_RISCOS)

void
set_bin(int fd)
{
}

int
c_pipe(int *fd)
{
	return pipe(fd);
}

#elif defined(CONFIG_OS2) || defined(CONFIG_WIN32)

void
set_bin(int fd)
{
	setmode(fd, O_BINARY);
}

int
c_pipe(int *fd)
{
	int r = pipe(fd);

	if (!r) {
		set_bin(fd[0]);
		set_bin(fd[1]);
	}

	return r;
}

#endif

/* Exec */

int
is_twterm(void) /* Check if it make sense to call a twterm. */
{
	static int tw = -1;

	if (tw == -1) tw = !!getenv("TWDISPLAY");

	return tw;
}

int
is_gnuscreen(void)
{
	static int screen = -1;

	if (screen == -1) screen = !!getenv("STY");

	return screen;
}


#if defined(CONFIG_UNIX) || defined(CONFIG_WIN32)

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) {
		/* Paraphrased from debian bug 228977:
		 *
		 * It is not enough to simply check the DISPLAY env variable,
		 * as it is pretty legal to have a DISPLAY set. While these
		 * days this practice is pretty uncommon, it still makes sense
		 * sometimes, especially for people who prefer the text mode
		 * for some reason. Only relying on DISPLAY will results in bad
		 * codes being written to the terminal.
		 *
		 * Any real xterm derivative sets WINDOWID as well.
		 * Unfortunately, konsole is an exception, and it needs to be
		 * checked for separately.
		 *
		 * FIXME: The code below still fails to detect some terminals
		 * that do support a title (like the popular PuTTY ssh client).
		 * In general, proper xterm detection is a nightmarish task...
		 *
		 * -- Adam Borowski <kilobyte@mimuw.edu.pl> */
		unsigned char *display = getenv("DISPLAY");
		unsigned char *windowid = getenv("WINDOWID");

		if (!windowid || !*windowid)
			windowid = getenv("KONSOLE_DCOP_SESSION");
		xt = (display && *display && windowid && *windowid);
	}

	return xt;
}

#endif

unsigned int resize_count = 0;

#ifndef CONFIG_OS2

#if !(defined(CONFIG_BEOS) && defined(HAVE_SETPGID)) && !defined(CONFIG_WIN32)

int
exe(unsigned char *path)
{
	return system(path);
}

#endif

unsigned char *
get_clipboard_text(void)	/* !!! FIXME */
{
	unsigned char *ret;

	/* GNU Screen's clipboard */
	if (is_gnuscreen()) {
		struct string str;

		if (!init_string(&str)) return;

		add_to_string(&str, "screen -X paste .");
		if (str.length) exe(str.source);
		if (str.source) done_string(&str);
	}

	ret = mem_alloc(1);
	if (ret) ret[0] = 0;
	return ret;
}

void
set_clipboard_text(unsigned char *data)
{
	/* GNU Screen's clipboard */
	if (is_gnuscreen()) {
		struct string str;

		if (!init_string(&str)) return;

		add_to_string(&str, "screen -X register . ");
		add_shell_quoted_to_string(&str, data, strlen(data));

		if (str.length) exe(str.source);
		if (str.source) done_string(&str);
	}

	/* TODO: internal clipboard */
}

/* Set xterm-like term window's title. */
void
set_window_title(unsigned char *title)
{
	unsigned char *s;
	int xsize, ysize;
	int j = 0;

#ifndef HAVE_SYS_CYGWIN_H
	/* Check if we're in a xterm-like terminal. */
	if (!is_xterm() && !is_gnuscreen()) return;
#endif

	/* Retrieve terminal dimensions. */
	get_terminal_size(0, &xsize, &ysize);

	/* Check if terminal width is reasonnable. */
	if (xsize < 1 || xsize > 1024) return;

	/* Allocate space for title + 3 ending points + null char. */
	s = mem_alloc(xsize + 3 + 1);
	if (!s) return;

	/* Copy title to s if different from NULL */
	if (title) {
		int i;

		/* We limit title length to terminal width and ignore control
		 * chars if any. Note that in most cases window decoration
		 * reduces printable width, so it's just a precaution. */
		/* Note that this is the right place where to do it, since
		 * potential alternative set_window_title() routines might
		 * want to take different precautions. */
		for (i = 0; title[i] && i < xsize; i++) {
			/* 0x80 .. 0x9f are ISO-8859-* control characters.
			 * In some other encodings they could be used for
			 * legitimate characters, though (ie. in Kamenicky).
			 * We should therefore maybe check for these only
			 * if the terminal is running in an ISO- encoding. */
			if (iscntrl(title[i]) || (title[i] & 0x7f) < 0x20
			    || title[i] == 0x7f)
				continue;

			s[j++] = title[i];
		}

		/* If title is truncated, add "..." */
		if (i == xsize) {
			s[j++] = '.';
			s[j++] = '.';
			s[j++] = '.';
		}
	}
	s[j] = '\0';

	/* Send terminal escape sequence + title string */
	printf("\033]0;%s\a", s);

#if 0
	/* Miciah don't like this so it is disabled because it changes the
	 * default window name. --jonas */
	/* Set the GNU screen window name */
	if (is_gnuscreen())
		printf("\033k%s\033\134", s);
#endif

	fflush(stdout);

	mem_free(s);
}

#ifdef HAVE_X11
static int x_error = 0;

static int
catch_x_error(void)
{
	x_error = 1;
	return 0;
}
#endif

unsigned char *
get_window_title(void)
{
#ifdef HAVE_X11
	/* Following code is stolen from our beloved vim. */
	unsigned char *winid;
	Display *display;
	Window window, root, parent, *children;
	XTextProperty text_prop;
	Status status;
	unsigned int num_children;
	unsigned char *ret = NULL;

	if (!is_xterm())
		return NULL;

	winid = getenv("WINDOWID");
	if (!winid)
		return NULL;
	window = (Window) atol(winid);
	if (!window)
		return NULL;

	display = XOpenDisplay(NULL);
	if (!display)
		return NULL;

	/* If WINDOWID is bad, we don't want X to abort us. */
	x_error = 0;
	XSetErrorHandler((int (*)(Display *, XErrorEvent *)) catch_x_error);

	status = XGetWMName(display, window, &text_prop);
	/* status = XGetWMIconName(x11_display, x11_window, &text_prop); */
	while (!x_error && (!status || !text_prop.value)) {
		if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
			break;
		if (children)
			XFree((void *) children);
		if (parent == root || parent == 0)
			break;
		window = parent;
		status = XGetWMName(display, window, &text_prop);
	}

	if (!x_error && status && text_prop.value) {
		ret = stracpy(text_prop.value);
		XFree(text_prop.value);
	}

	XCloseDisplay(display);

	return ret;
#else
	/* At least reset the window title to a blank one. */
	return stracpy("");
#endif
}

int
resize_window(int width, int height, int old_width, int old_height)
{
#ifdef HAVE_X11
	/* Following code is stolen from our beloved vim. */
	unsigned char *winid;
	Display *display;
	Window window;
	Status status;
	XWindowAttributes attributes;

	if (!is_xterm())
		return -1;

	winid = getenv("WINDOWID");
	if (!winid)
		return -1;
	window = (Window) atol(winid);
	if (!window)
		return -1;

	display = XOpenDisplay(NULL);
	if (!display)
		return -1;

	/* If WINDOWID is bad, we don't want X to abort us. */
	x_error = 0;
	XSetErrorHandler((int (*)(Display *, XErrorEvent *)) catch_x_error);

	status = XGetWindowAttributes(display, window, &attributes);

	while (!x_error && !status) {
		Window root, parent, *children;
		unsigned int num_children;

		if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
			break;
		if (children)
			XFree((void *) children);
		if (parent == root || parent == 0)
			break;
		window = parent;
		status = XGetWindowAttributes(display, window, &attributes);
	}

	if (!x_error && status) {
		double ratio_width = (double) attributes.width  / old_width;
		double ratio_height = (double) attributes.height / old_height;

		width  = (int) ((double) width * ratio_width);
		height = (int) ((double) height * ratio_height);

		status = XResizeWindow(display, window, width, height);
		while (!x_error && !status) {
			Window root, parent, *children;
			unsigned int num_children;

			if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
				break;
			if (children)
				XFree((void *) children);
			if (parent == root || parent == 0)
				break;
			window = parent;
			status = XResizeWindow(display, window, width, height);
		}
	}

	XCloseDisplay(display);

	return 0;
#else
	return -1;
#endif
}

#endif


/* Threads */

#if defined(HAVE_BEGINTHREAD) || defined(CONFIG_BEOS)

struct tdata {
	void (*fn)(void *, int);
	int h;
	unsigned char data[1];
};

void
bgt(struct tdata *t)
{
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
	t->fn(t->data, t->h);
	write(t->h, "x", 1);
	close(t->h);
	free(t);
}

#else

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	pid_t pid;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	pid = fork();
	if (!pid) {
		struct terminal *term;

		/* Close input in this thread; otherwise, if it will live
		 * longer than its parent, it'll block the terminal until it'll
		 * quit as well; this way it will hopefully just die unseen and
		 * in background, causing no trouble. */
		/* Particularly, when async dns resolving was in progress and
		 * someone quitted ELinks, it could make a delay before the
		 * terminal would be really freed and returned to shell. */
		foreach (term, terminals)
			if (term->fdin > 0)
				close(term->fdin);

		close(p[0]);
		fn(ptr, p[1]);
		write(p[1], "x", 1);
		close(p[1]);
		/* We use _exit() here instead of exit(), see
		 * http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC6 for
		 * reasons. Fixed by Sven Neumann <sven@convergence.de>. */
		_exit(0);
	}
	if (pid == -1) {
		close(p[0]);
		close(p[1]);
		return -1;
	}

	close(p[1]);
	return p[0];
}

#endif


#ifndef OS2_MOUSE
void
want_draw(void)
{
}

void
done_draw(void)
{
}
#endif


#if !defined(CONFIG_WIN32)
int
get_output_handle(void)
{
	return 1;
}

int
get_ctl_handle(void)
{
	static int fd = -1;

	if (isatty(0)) return 0;
	if (fd < 0) fd = open("/dev/tty", O_RDONLY);
	return fd;
}
#endif


#if !defined(CONFIG_BEOS) && !(defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD)) \
	&& !defined(CONFIG_WIN32)

int
get_input_handle(void)
{
	return get_ctl_handle();
}

#endif

#ifndef CONFIG_WIN32

void
init_osdep(void)
{
#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
}

#endif

#if defined(CONFIG_UNIX) || defined(CONFIG_OS2) || defined(CONFIG_RISCOS)

void
terminate_osdep(void)
{
}

#endif

#ifndef CONFIG_BEOS

void
block_stdin(void)
{
}

void
unblock_stdin(void)
{
}

#endif


void
elinks_cfmakeraw(struct termios *t)
{
#ifdef HAVE_CFMAKERAW
	cfmakeraw(t);
#ifdef VMIN
	t->c_cc[VMIN] = 1; /* cfmakeraw() is broken on AIX --mikulas */
#endif
#else
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
#endif
}

#if !defined(CONFIG_MOUSE) || (!defined(CONFIG_GPM) && !defined(CONFIG_SYSMOUSE) && !defined(OS2_MOUSE))

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	return NULL;
}

void
unhandle_mouse(void *data)
{
}

void
suspend_mouse(void *data)
{
}

void
resume_mouse(void *data)
{
}

#endif

#ifndef CONFIG_WIN32
/* Create a bitmask consisting from system-independent envirnoment modifiers.
 * This is then complemented by system-specific modifiers in an appropriate
 * get_system_env() routine. */
static int
get_common_env(void)
{
	int env = 0;

	if (is_xterm()) env |= ENV_XWIN;
	if (is_twterm()) env |= ENV_TWIN;
	if (is_gnuscreen()) env |= ENV_SCREEN;

	/* ENV_CONSOLE is always set now and indicates that we are working w/ a
	 * displaying-capable character-adressed terminal. Sounds purely
	 * theoretically now, but it already makes some things easier and it
	 * could give us interesting opportunities later (after graphical
	 * frontends will be introduced, when working in some mysterious daemon
	 * mode or who knows what ;). --pasky */
	env |= ENV_CONSOLE;

	return env;
}
#endif

#if defined(CONFIG_UNIX) || defined(CONFIG_RISCOS)
int
get_system_env(void)
{
	return get_common_env();
}
#endif


int
can_resize_window(int environment)
{
	return !!(environment & (ENV_OS2VIO | ENV_XWIN));
}

#ifndef CONFIG_OS2
int
can_open_os_shell(int environment)
{
	return 1;
}

void
set_highpri(void)
{
}
#endif


unsigned char *
get_system_str(int xwin)
{
	return xwin ? SYSTEM_STR "-xwin" : SYSTEM_STR;
}
