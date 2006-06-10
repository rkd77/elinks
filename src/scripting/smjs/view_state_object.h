#ifndef EL__SCRIPTING_SMJS_VIEW_STATE_OBJECT_H
#define EL__SCRIPTING_SMJS_VIEW_STATE_OBJECT_H

struct view_state;

JSObject *smjs_get_view_state_object(struct view_state *vs);

void smjs_init_view_state_interface(void);

#endif

