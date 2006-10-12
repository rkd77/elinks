#ifndef EL__MAIN_EVENT_H
#define EL__MAIN_EVENT_H

#include <stdarg.h>

#define EVENT_NONE (-1)


/* This enum is returned by each event hook and determines whether we should
 * go on in the chain or finish the event processing. You want to always
 * return EVENT_HOOK_STATUS_NEXT. */
/* More about EVENT_HOOK_STATUS_LAST - it means the event will get stuck on YOU
 * and won't ever get to any other hooks queued behind you. This is usually not
 * what you want. When I have plugin doing X with some document being loaded,
 * and then script doing Y with it and finally some internal gadget doing some
 * final touching of the document (say, colors normalization or so, I'm pulling
 * this off my head), you definitively want all of them to happen, no matter if
 * they are all successful or all unsuccessful or part of them successful. The
 * only exceptions I can think of are:
 *
 * * I really messed something up. Ie. I somehow managed to destroy the
 * document I got on input, but at least I know I did. Others don't and they
 * will crash, and the document is of no use anyway. So let me be the last one.
 *
 * * I discarded the event. Say I'm children-protection plugin and the innocent
 * kid drove me (unintentionally, of course) to some adult site. So I catch
 * the appropriate event as long as I know it is bad, and discard it. No luck,
 * Jonny. Oh, wait, you are going to write *own* plugin...?!
 *
 * * I transformed the even to another one. Say user pressed a key and I'm the
 * keybinding hook. So I've found the key in my keybinding table, thus I catch
 * it, lazy_trigger (TODO) the appropriate specific event (ie. "quit") and of
 * course discard the original one.
 *
 * --pasky */
enum evhook_status {
	EVENT_HOOK_STATUS_NEXT,
	EVENT_HOOK_STATUS_LAST,
};

/* The event hook prototype. Abide. */
typedef enum evhook_status (*event_hook_T)(va_list ap, void *data);

/* This is convenience macro for hooks. Not all hooks may use all of their
 * parameters, but they usually have to fetch all of them. This silences
 * compiler ranting about unused variables then. It is recommended to run this
 * upon all the parameters (in case some of the get unused in future) like:
 *
 * evhook_use_params(param1 && param2 && param3);
 *
 * The compiler is probably going to optimize it away anyway. */
#define evhook_use_params(x) if (0 && (x)) ;


/*** The life of events */

/* This registers an event of name @name, allocating an id number for it. */
/* The function returns the id or negative number upon error. */
int register_event(unsigned char *name);

/* This unregisters an event number @event, freeing the resources it
 * occupied, chain of associated hooks and unallocating the event id for
 * further recyclation.
 * Bug 810: unregister_event has not yet been implemented. */
int unregister_event(int event);

int register_event_hook(int id, event_hook_T callback, int priority, void *data);

void unregister_event_hook(int id, event_hook_T callback);


/*** Interface for table driven event hooks maintainance */

struct event_hook_info {
	unsigned char *name;
	int priority;
	event_hook_T callback;
	void *data;
};

#define NULL_EVENT_HOOK_INFO { NULL, 0, NULL, NULL }

void register_event_hooks(struct event_hook_info *hooks);
void unregister_event_hooks(struct event_hook_info *hooks);

/*** The events resolver */

/* This looks up the events table and returns the event id associated
 * with a given event @name. The event id is guaranteed not to change
 * during the event lifetime (that is, between its registration and
 * unregistration), thus it may be cached intermediatelly. */
/* It returns the event id on success or a negative number upon failure
 * (ie. there is no such event). */
int get_event_id(unsigned char *name);

/* This looks up the events table and returns the name of a given event
 * @id. */
/* It returns the event name on success (statically allocated, you are
 * not permitted to modify it) or NULL upon failure (ie. there is no
 * such event). */
unsigned char *get_event_name(int id);

#define set_event_id(event, name) 			\
	do { 						\
		if (event == EVENT_NONE) 		\
			event = get_event_id(name);	\
	} while (0)


/*** The events generator */

void trigger_event(int id, ...);

void trigger_event_name(unsigned char *name, ...);

/*** The very events subsystem itself */

void init_event(void);

void done_event(void);


#endif
