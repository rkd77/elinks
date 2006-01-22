#ifndef EL__MAIN_OBJECT_H
#define EL__MAIN_OBJECT_H

#include "util/lists.h"

#if 0
#define DEBUG_REFCOUNT
#endif

struct object {
	int refcount;
#ifdef CONFIG_DEBUG
	unsigned char *name;
#endif
};

#define OBJECT_HEAD(type)						\
	LIST_HEAD(type);						\
	struct object object

struct object_head {
	OBJECT_HEAD(struct object *);
};

#ifdef DEBUG_REFCOUNT
#include "util/error.h"
#ifdef CONFIG_DEBUG
#define object_lock_debug(obj, info) \
	DBG("object %s[%p] lock %s to %d", (obj)->object.name, obj,	\
	    info, (obj)->object.refcount)
#else
#define object_lock_debug(obj, info) \
	DBG("object %p lock %s to %d", obj, info, (obj)->object.refcount)
#endif /* CONFIG_DEBUG */
#else
#define object_lock_debug(obj, info)
#endif /* DEBUG_REFCOUNT */

#ifdef CONFIG_DEBUG
#include "util/error.h"
#define object_sanity_check(obj)					\
	do {								\
		assert(obj);						\
		assertm((obj)->object.refcount >= 0,			\
			"Object %s[%p] refcount underflow.",		\
			(obj)->object.name, obj);			\
		if_assert_failed (obj)->object.refcount = 0;		\
	} while (0)

#define object_set_name(obj, objname)					\
	do { (obj)->object.name = (objname); } while (0)
#define INIT_OBJECT(name)	{ 0, name }
#else
#define object_sanity_check(obj)
#define object_set_name(obj, name)
#define INIT_OBJECT(name)	{ 0 }
#endif /* CONFIG_DEBUG */

#define get_object_refcount(obj) ((obj)->object.refcount)
#define is_object_used(obj) (!!(obj)->object.refcount)

#define object_lock(obj)						\
	do {								\
		object_sanity_check(obj);				\
		(obj)->object.refcount++;				\
		object_lock_debug(obj, "incremented");			\
	} while (0)

#define object_unlock(obj)						\
	do {								\
		(obj)->object.refcount--;				\
		object_lock_debug(obj, "decremented");			\
		object_sanity_check(obj);				\
	} while (0)

/* Please keep this one. It serves for debugging. --Zas */
#define object_nolock(obj, name)					\
	do {								\
		object_set_name(obj, name);				\
		object_sanity_check(obj);				\
		object_lock_debug(obj, "initialized");			\
	} while (0)

#endif
