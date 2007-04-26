#ifndef EL__ECMASCRIPT_SEE_WINDOW_H
#define EL__ECMASCRIPT_SEE_WINDOW_H

struct SEE_object;
struct SEE_interpreter;
struct ecmascript_interpreter;
struct string;
struct view_state;


struct js_window_object {
	struct SEE_object object;
	struct view_state *vs;
	struct SEE_object *alert;
	struct SEE_object *open;
	struct SEE_object *setTimeout;
};

struct global_object {
	struct SEE_interpreter interp;
	/* used by setTimeout */
	struct ecmascript_interpreter *interpreter;
	struct js_window_object *win;
	struct string *ret;
	int exec_start;
	int max_exec_time;
};

void init_js_window_object(struct ecmascript_interpreter *);
void checktime(struct SEE_interpreter *interp);

#endif
