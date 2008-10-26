/* BeOS system-specific routines. */

/* Note that this file is currently unmaintained and basically dead. Noone
 * cares about BeOS support, apparently. This file may yet survive for some
 * time, but it will probably be removed if noone will adopt it. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <be/kernel/OS.h>

#include "elinks.h"

#include "osdep/beos/beos.h"
#include "terminal/terminal.h"
#include "util/lists.h"


/* Misc trivial stuff */

int
is_xterm(void)
{
	return 0;
}

int
get_system_env(void)
{
	int env = get_common_env();
	unsigned char *term = getenv("TERM");

	if (!term || (c_toupper(term[0]) == 'B' && c_toupper(term[1]) == 'E'))
		env |= ENV_BE;

	return env;
}


/* Threads */

int thr_sem_init = 0;
sem_id thr_sem;

struct active_thread {
	LIST_HEAD(struct active_thread);

	thread_id tid;
	void (*fn)(void *);
	void *data;
};

INIT_LIST_OF(struct active_thread, active_threads);

int32
started_thr(void *data)
{
	struct active_thread *thrd = data;

	thrd->fn(thrd->data);
	if (acquire_sem(thr_sem) < B_NO_ERROR) return 0;
	del_from_list(thrd);
	free(thrd);
	release_sem(thr_sem);

	return 0;
}

int
start_thr(void (*fn)(void *), void *data, unsigned char *name)
{
	struct active_thread *thrd;
	int tid;

	if (!thr_sem_init) {
		thr_sem = create_sem(0, "thread_sem");
		if (thr_sem < B_NO_ERROR) return -1;
		thr_sem_init = 1;
	} else if (acquire_sem(thr_sem) < B_NO_ERROR) return -1;

	thrd = malloc(sizeof(*thrd));
	if (!thrd) goto rel;
	thrd->fn = fn;
	thrd->data = data;
	thrd->tid = spawn_thread(started_thr, name, B_NORMAL_PRIORITY, thrd);
	tid = thrd->tid;

	if (tid < B_NO_ERROR) {
		free(thrd);

rel:
		release_sem(thr_sem);
		return -1;
	}

	resume_thread(thrd->tid);
	add_to_list(active_threads, thrd);
	release_sem(thr_sem);

	return tid;
}

void
terminate_osdep(void)
{
	LIST_OF(struct active_thread) *p;
	struct active_thread *thrd;

	if (acquire_sem(thr_sem) < B_NO_ERROR) return;
	foreach (thrd, active_threads) kill_thread(thrd->tid);

	/* Cannot use free_list(active_threads) because it would call
	 * mem_free(p) and we need free(p).  */
	while ((p = active_threads.next) != &active_threads) {
		del_from_list(p);
		free(p);
	}
	release_sem(thr_sem);
}

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	struct tdata *t;

	if (c_pipe(p) < 0) return -1;

	t = malloc(sizeof(*t) + l);
	if (!t) return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (start_thr((void (*)(void *)) bgt, t, "elinks_thread") < 0) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}

int ihpipe[2];

int inth;

void
input_handle_th(void *p)
{
	char c;
	int b = 0;

	setsockopt(ihpipe[1], SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
	while (1) if (read(0, &c, 1) == 1) be_write(ihpipe[1], &c, 1);
}

int
get_input_handle(void)
{
	static int h = -1;

	if (h >= 0) return h;
	if (be_pipe(ihpipe) < 0) return -1;
	if ((inth = start_thr(input_handle_th, NULL, "input_thread")) < 0) {
		closesocket(ihpipe[0]);
		closesocket(ihpipe[1]);
		return -1;
	}
	return h = ihpipe[0];
}

void
block_stdin(void)
{
	suspend_thread(inth);
}

void
unblock_stdin(void)
{
	resume_thread(inth);
}

#if 0
int ohpipe[2];

#define O_BUF	16384

void
output_handle_th(void *p)
{
	char *c = malloc(O_BUF);
	int r, b = 0;

	if (!c) return;
	setsockopt(ohpipe[1], SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
	while ((r = be_read(ohpipe[0], c, O_BUF)) > 0) write(1, c, r);
	free(c);
}

int
get_output_handle(void)
{
	static int h = -1;

	if (h >= 0) return h;
	if (be_pipe(ohpipe) < 0) return -1;
	if (start_thr(output_handle_th, NULL, "output_thread") < 0) {
		closesocket(ohpipe[0]);
		closesocket(ohpipe[1]);
		return -1;
	}
	return h = ohpipe[1];
}
#endif


#if defined(HAVE_SETPGID)

int
exe(unsigned char *path)
{
	pid_t pid = fork();

	if (!pid) {
		setpgid(0, 0);
		system(path);
		_exit(0);
	}

	if (pid > 0) {
		int s;

		waitpid(pid, &s, 0);
	} else
		return system(path);

	return 0;
}

#endif
