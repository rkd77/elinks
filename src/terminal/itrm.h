#ifndef EL__TERMINAL_ITRM_H
#define EL__TERMINAL_ITRM_H


#define ITRM_OUT_QUEUE_SIZE	16384
#define ITRM_IN_QUEUE_SIZE	16

struct itrm_queue {
	unsigned char *data;

	/* The amount of data in the queue, in bytes.  This may be
	 * less than the amount of memory allocated for the buffer;
	 * struct itrm_queue does not keep track of that, and has
	 * no global policy on whether the buffer can be resized.  */
	int len;
};

/* Things coming into an itrm, whether from the terminal or from the
 * master.  */
struct itrm_in {
	/* A file descriptor for the standard input.  In some ports,
	 * this is the terminal device itself; in others, this is a
	 * pipe from an input thread.  In principle, the data format
	 * depends on the terminal.  */
	int std;

	/* In a slave process, a file descriptor for a socket from
	 * which it reads data sent by the master process.  The other
	 * end of the socket connection is terminal.fdout in the
	 * master process.  The format of this data is almost the same
	 * as could be sent to the terminal (via itrm.out.std), but
	 * there are special commands that begin with a null byte.
	 *
	 * In the master process, @sock is the same as @ctl, but
	 * nothing actually uses it.  */
	int sock;

	/* A file descriptor for controlling the standard input.  This
	 * is always the terminal device itself, thus the same as @std
	 * in some ports.  ELinks doesn't read or write with this file
	 * descriptor; it only does things like tcsetattr.  */
	int ctl;

	/* Bytes that have been received from @std but not yet
	 * converted to events.  queue.data is allocated for
	 * ITRM_IN_QUEUE_SIZE bytes and never resized.  The itrm
	 * layer cannot parse control sequences longer than that.  */
	struct itrm_queue queue;
};

/* Things going out from an itrm, whether to the terminal or to the
 * master.  */
struct itrm_out {
	/* A file descriptor for the standard output.  In some ports,
	 * this is the terminal device itself; in others, this is a
	 * pipe to an output thread.  In principle, the data format
	 * depends on the terminal; but see bug 96.  */
	int std;

	/* A file descriptor for a pipe or socket to which this
	 * process sends input events.  The other end of the pipe or
	 * socket connection is terminal.fdin in the master process.
	 * If the connection is from the master process to itself, it
	 * uses a pipe; otherwise a socket.  The events are formatted
	 * as struct term_event, but at the beginning of the
	 * connection, a struct terminal_info and extra data is also
	 * sent.  */
	int sock;

	/* Bytes that should be written to @sock.  They will be
	 * written when select() indicates the write won't block.  To
	 * add data here, call itrm_queue_event(), which reallocates
	 * queue.data if appropriate.  The size of this queue is
	 * unrelated to ITRM_OUT_QUEUE_SIZE.  */
	struct itrm_queue queue;
};

/* A connection between a terminal and a master ELinks process.
 * Normally, only one struct itrm exists in each master or slave
 * process, and the global pointer @ditrm (not declared here)
 * points to it.  */
struct itrm {
	struct itrm_in in;		/* Input */
	struct itrm_out out;		/* Output */

	timer_id_T timer;		/* ESC timeout timer */
	struct termios t;		/* For restoring original attributes */
	void *mouse_h;			/* Mouse handle */
	unsigned char *orig_title;	/* For restoring window title */

	unsigned int blocked:1;		/* Whether it was blocked */
	unsigned int altscreen:1;	/* Whether to use alternate screen */
	unsigned int touched_title:1;	/* Whether the term title was changed */
	unsigned int remote:1;		/* Whether it is a remote session */
};

#endif
