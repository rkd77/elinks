/* Event handling functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "main/event.h"
#include "util/error.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


/* First, we should set some terminology:
 *
 * o event -> Message being [triggerred] by some ELinks part and [catched]
 *   by various random other ones.
 *
 * o event hook -> Device being deployed by various ELinks parts,
 *   associated with certain [event It [catches] that event by having a
 *   handler executed when the [event] is triggerred.
 *
 * o event chain -> Line of [event hook]s associated with a given [event
 *   The [hook]s are ordered by [priority Each hook returns whenever should
 *   the chain continue or that no other events in the chain should be
 *   triggered (TODO).
 */

struct event_handler {
	/* The function to be called with the event data. */
	event_hook_T callback;

	/* The @priority of this handler. */
	int priority;

	/* Handler specific data. */
	void *data;
};

struct event {
	/* The event name has to be unique. */
	unsigned char *name;

	/* There are @count event @handlers all ordered by priority. */
	struct event_handler *handlers;
	unsigned int count;

	/* The unique event id and position in events. */
	int id;
};

static struct event *events = NULL;
static unsigned int eventssize = 0;
static struct hash *event_hash = NULL;

/* TODO: This should be tuned to the number of events.  When we will have a lot
 * of them, this should be some big enough number to reduce unnecessary
 * slavery of CPU on the startup. Then after all main modules will be
 * initialized and their events will be registered, we could call something
 * like adjust_events_list() which will tune it to the exactly needed number.
 * This should be also called after each new plugin loaded. */
#define EVENT_GRANULARITY 0x07

#define realloc_events(ptr, size) \
	mem_align_alloc(ptr, (size), (size) + 1, EVENT_GRANULARITY)

static inline int
invalid_event_id(register int id)
{
	return (id < 0 || id >= eventssize || id == EVENT_NONE);
}

int
register_event(unsigned char *name)
{
	int id = get_event_id(name);
	struct event *event;
	int namelen;

	if (id != EVENT_NONE) return id;

	event = events;
	if (!realloc_events(&events, eventssize)) return EVENT_NONE;

	/* If @events got relocated update the hash. */
	if (event != events) {
		for (id = 0; id < eventssize; id++) {
			struct hash_item *item;
			int len = strlen(events[id].name);

			item = get_hash_item(event_hash, events[id].name, len);

			if (item) item->value = &events[id];
		}
	}

	event = &events[eventssize];

	namelen = strlen(name);
	event->name = memacpy(name, namelen);
	if (!event->name) return EVENT_NONE;

	if (!add_hash_item(event_hash, event->name, namelen, event)) {
		mem_free(event->name);
		event->name = NULL;
		return EVENT_NONE;
	}

	event->handlers = NULL;
	event->count = 0;
	event->id = eventssize++;

	return event->id;
}

int
get_event_id(unsigned char *name)
{
	struct hash_item *item;
	int namelen;

	assertm(name && name[0], "Empty or missing event name");
	if_assert_failed return EVENT_NONE;

	if (!event_hash) return EVENT_NONE;

	namelen = strlen(name);
	item = get_hash_item(event_hash, name, namelen);
	if (item) {
		struct event *event = item->value;

		assertm(event != NULL, "Hash item with no value");
		if_assert_failed return EVENT_NONE;

		return event->id;
	}

	return EVENT_NONE;
}

unsigned char *
get_event_name(int id)
{
	if (invalid_event_id(id)) return NULL;

	return events[id].name;
}

static void
trigger_event_va(int id, va_list ap_init)
{
	int i;
	struct event_handler *ev_handler;

	if (invalid_event_id(id)) return;

	ev_handler = events[id].handlers;
	for (i = 0; i < events[id].count; i++, ev_handler++) {
		enum evhook_status ret;
		va_list ap;

		VA_COPY(ap, ap_init);
		ret = ev_handler->callback(ap, ev_handler->data);
		va_end(ap);

		if (ret == EVENT_HOOK_STATUS_LAST) return;
	}
}

void
trigger_event(int id, ...)
{
	va_list ap;

	va_start(ap, id);
	trigger_event_va(id, ap);
	va_end(ap);
}

void
trigger_event_name(unsigned char *name, ...)
{
	va_list ap;
	int id = get_event_id(name);

	va_start(ap, name);
	trigger_event_va(id, ap);
	va_end(ap);
}

static inline void
move_event_handler(struct event *event, int to, int from)
{
	int d = int_max(to, from);

	memmove(&event->handlers[to], &event->handlers[from],
		(event->count - d) * sizeof(*event->handlers));
}

int
register_event_hook(int id, event_hook_T callback, int priority, void *data)
{
	struct event *event;
	int i;

	assert(callback);
	if_assert_failed return EVENT_NONE;

	if (invalid_event_id(id)) return EVENT_NONE;

	event = &events[id];

	for (i = 0; i < event->count; i++)
		if (event->handlers[i].callback == callback) break;

	if (i == event->count) {
		struct event_handler *eh;

		eh = mem_realloc(event->handlers,
				 (event->count + 1) * sizeof(*event->handlers));

		if (!eh) return EVENT_NONE;

		event->handlers = eh;
		event->count++;
	} else {
		move_event_handler(event, i, i + 1);
	}

	for (i = 0; i < event->count - 1; i++)
		if (event->handlers[i].priority < priority) break;

	move_event_handler(event, i + 1, i);

	event->handlers[i].callback = callback;
	event->handlers[i].priority = priority;
	event->handlers[i].data = data;

	return id;
}

void
unregister_event_hook(int id, event_hook_T callback)
{
	struct event *event;

	assert(callback);
	if_assert_failed return;

	if (invalid_event_id(id)) return;

	event = &events[id];
	if (event->handlers) {
		int i;

		for (i = 0; i < event->count; i++) {
			if (event->handlers[i].callback != callback)
				continue;

			move_event_handler(event, i, i + 1);
			event->count--;
			if (!event->count) {
				mem_free(event->handlers);
				event->handlers = NULL;
			} else {
				struct event_handler *eh;

				eh = mem_realloc(event->handlers,
						 event->count * sizeof(*event->handlers));
				if (eh) event->handlers = eh;
			}

			break;
		}
	}
}

void
register_event_hooks(struct event_hook_info *hooks)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id = register_event(hooks[i].name);

		if (id == EVENT_NONE) continue;

		register_event_hook(id, hooks[i].callback, hooks[i].priority,
		                    hooks[i].data);
	}
}

void
unregister_event_hooks(struct event_hook_info *hooks)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id = get_event_id(hooks[i].name);

		if (id == EVENT_NONE) continue;

		unregister_event_hook(id, hooks[i].callback);
	}
}

void
init_event(void)
{
	event_hash = init_hash8();
}

void
done_event(void)
{
	int i;

	for (i = 0; i < eventssize; i++) {
		mem_free_if(events[i].handlers);
		mem_free(events[i].name);
	}

	mem_free_set(&events, NULL);

	if (event_hash)	free_hash(&event_hash);
	eventssize = 0;
}
