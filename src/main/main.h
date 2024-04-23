#ifndef EL__MAIN_MAIN_H
#define EL__MAIN_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

enum retval {
	RET_OK,		/* All is well */
	RET_ERROR,	/* Failed to fetch URL or write document when dumping */
	RET_SIGNAL,	/* Catched SIGTERM which forced program to stop */
	RET_SYNTAX,	/* Cmdline syntax error or bad or missing dump URL */
	RET_FATAL,	/* Fatal error occurred during initialization */
	RET_PING,	/* --remote "ping()" found no running ELinkses */
	RET_REMOTE,	/* --remote failed to connect to a running ELinks */
	RET_COMMAND,	/* Used internally for exiting from cmdline commands */
};

struct program {
	int terminate;
	enum retval retval;
	char *path;
	unsigned int testjs:1;
};

extern struct program program;
extern pid_t master_pid;

void shrink_memory(int);
void parse_options_again(void);

#ifdef __cplusplus
}
#endif

#endif
