/* $Id: itrm.h,v 1.1 2005/09/14 10:04:54 zas Exp $ */

#ifndef EL__TERMINAL_ITRM_H
#define EL__TERMINAL_ITRM_H


#define ITRM_OUT_QUEUE_SIZE	16384
#define ITRM_IN_QUEUE_SIZE	16

struct itrm_queue {
	unsigned char *data;
	int len;
};

struct itrm_in {
	int std;
	int sock;
	int ctl;
	struct itrm_queue queue;
};

struct itrm_out {
	int std;
	int sock;
	struct itrm_queue queue;
};

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
