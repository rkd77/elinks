
#ifndef EL__VIEWER_TEXT_MARKS_H
#define EL__VIEWER_TEXT_MARKS_H

struct module;
struct view_state;

void goto_mark(unsigned char mark, struct view_state *vs);
void set_mark(unsigned char mark, struct view_state *vs);

extern struct module viewer_marks_module;

#endif
