
#ifndef EL__VIEWER_TEXT_FESTIVAL_H
#define EL__VIEWER_TEXT_FESTIVAL_H

struct session;
struct document_view;

struct fest {
	struct document_view *doc_view;
	int line;
	int in;
	int out;
	int festival_or_flite;
	unsigned int running:1;
};

#ifdef HAVE_FORK
extern struct fest festival;

void run_festival(struct session *, struct document_view *);
#endif

#endif
