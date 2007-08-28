#ifndef EL__UTIL_LISTS_H
#define EL__UTIL_LISTS_H

#include "util/error.h" /* do_not_optimize_here() */

/* BEWARE! You MAY NOT use ternary operator as parameter to there functions,
 * because they are likely to take & of the parameter. Worst of all, it will
 * work with gcc. But nowhere else (at least not w/ tcc). */

/* Note that this whole lists implementation code is severely broken. All of
 * it is a single huge violation of C aliasing rules, just accessing things
 * like we do here is totally prohibited and bound to generate segfaults in
 * proportion with rising optimization level and gcc version.
 *
 * Fixing this would be a nice and needed janitorial project. */

/** Lists debugging.
 * Two unsigned int magic number will be put before and after the next and
 * prev pointers, these will be check on list operations.
 * Some pointers are set to specific values after action. */
#ifdef CONFIG_DEBUG
#define LISTDEBUG
#endif


/* Fix namespace clash with system headers (like FreeBSD's, all hail BSD!). */
#undef LIST_HEAD
#undef list_head
#define list_head list_head_elinks


#ifndef LISTDEBUG

#define list_del_enforce(x) /* better don't */

#define list_magic_error(where, what) /* no magic */

#define list_magic_set(x) /* no magic */

#define list_magic_correct(x) (1)
#define list_magic_check(x, where) /* no magic */
#define list_magic_chkbool(x, where) (1)

struct list_head {
	void *next;
	void *prev;
};

struct xlist_head {
	struct xlist_head *next;
	struct xlist_head *prev;
};

#define NULL_LIST_HEAD NULL, NULL
#define D_LIST_HEAD(x) &x, &x
#define LIST_HEAD(x) x *next; x *prev
#define LIST_SET_MAGIC(x) list_magic_set(*(x))

#else /* LISTDEBUG */

#define LISTMAGIC1 ((void *) 0xdadababa)
#define LISTMAGIC2 ((void *) 0xd0d0b0b0)
#define LISTMAGIC3 ((void *) 0x25254545)

#define list_del_enforce(x) \
do { \
	/* Little hack: we put LISTMAGIC3 in prev */ \
	/* and the line number in next. Debugging purpose. */ \
	(x)->prev = LISTMAGIC3; \
	(x)->next = (void *) ((unsigned int) __LINE__); \
} while (0)


#define list_magic_error(where,what) INTERNAL("[%s] %s - bad list magic", where, #what)


#define list_magic_set(x) \
do { \
	(x).magic1 = LISTMAGIC1; \
	(x).magic2 = LISTMAGIC2; \
} while (0)


/** Backend for list_magic_check() and list_magic_chkbool(). */
#define list_magic_correct(x) ((x).magic1 == LISTMAGIC1 && (x).magic2 == LISTMAGIC2)

#define list_magic_check(x, where) \
do { \
	if (!list_magic_correct(*(x))) \
		list_magic_error(where, x); \
} while (0)

#define list_magic_chkbool(x, where) (list_magic_correct(x) || (list_magic_error(where, x), 1))


struct list_head {
	void *magic1;
	void *next;
	void *prev;
	void *magic2;
};

struct xlist_head {
	void *magic1;
	struct xlist_head *next;
	struct xlist_head *prev;
	void *magic2;
};


#define NULL_LIST_HEAD LISTMAGIC1, NULL, NULL, LISTMAGIC2
#define D_LIST_HEAD(x) LISTMAGIC1, &x, &x, LISTMAGIC2
#define LIST_HEAD(x) void *magic1; x *next; x *prev; void *magic2
#define LIST_SET_MAGIC(x) list_magic_set(*(x))

#endif /* LISTDEBUG */

/** A list intended to contain elements of a specific type.  The
 * @a element_T parameter currently serves as documentation only;
 * the compiler does not check that it matches.  Doxyfile defines
 * this macro differently in order to get better collaboration
 * diagrams.  */
#define LIST_OF(element_T) struct list_head

/** Define and initialize a list variable.  The @a element_T parameter
 * currently serves as documentation only; the compiler does not check
 * that it matches.  */
#define INIT_LIST_OF(element_T, x) LIST_OF(element_T) x = { D_LIST_HEAD(x) }

#ifdef HAVE_TYPEOF
#define list_typeof(x) typeof(x)
#else
#define list_typeof(x) struct xlist_head *
#endif /* HAVE_TYPEOF */


#define init_list(x) \
do { \
	list_magic_set(x); \
	do_not_optimize_here_gcc_3_x(&(x)); /* antialiasing ;) */ \
	(x).next = (x).prev = &(x); \
	do_not_optimize_here_gcc_3_x(&(x)); \
} while (0)


#define list_empty(x) (list_magic_chkbool(x, "list_empty") && (x).next == &(x))

#define list_is_singleton(x) \
	(list_magic_chkbool(x, "list_is_singleton") && (x).next == (x).prev)

#define list_has_prev(l,p) \
	(list_magic_chkbool(l, "list_has_prev") && (p)->prev !=  (void *) &(l))

#define list_has_next(l,p) \
	(list_magic_chkbool(l, "list_has_next") && (p)->next !=  (void *) &(l))

#define del_from_list(x) \
do { \
	list_magic_check(x, "del_from_list"); \
	do_not_optimize_here_gcc_2_7(x); \
	((struct list_head *) (x)->next)->prev = (x)->prev; \
	((struct list_head *) (x)->prev)->next = (x)->next; \
	list_del_enforce(x); \
	do_not_optimize_here_gcc_2_7(x); \
} while (0)

#define add_at_pos(p,x) \
do { \
	list_magic_check(p, "add_at_pos"); \
	list_magic_set(*(x)); \
	do_not_optimize_here_gcc_2_7(p); \
	(x)->next = (p)->next; \
	(x)->prev = (p); \
   	(p)->next = (x); \
   	(x)->next->prev = (x); \
	do_not_optimize_here_gcc_2_7(p); \
} while (0)


#define add_to_list(l,x) \
	add_at_pos((list_typeof(x)) &(l), (list_typeof(x)) (x))

#define add_to_list_end(l,x) \
	add_at_pos((list_typeof(x)) (l).prev, (list_typeof(x)) (x))

#define foreach(e,l) \
	for ((e) = (l).next; \
	     (list_typeof(e)) (e) != (list_typeof(e)) &(l); \
	     (e) = (e)->next)

#define foreachback(e,l) \
	for ((e) = (l).prev; \
	     (list_typeof(e)) (e) != (list_typeof(e)) &(l); \
	     (e) = (e)->prev)

#define foreachsafe(e, n, l) \
	for ((e) = (l).next, (n) = (e)->next; \
	     (list_typeof(e)) (e) != (list_typeof(e)) &(l); \
	     (e) = (n), (n) = (e)->next)

#define foreachbacksafe(e, p, l) \
	for ((e) = (l).prev, (p) = (e)->prev; \
	     (list_typeof(e)) (e) != (list_typeof(e)) &(l); \
	     (e) = (p), (p) = (e)->prev)

#define free_list(l) \
do { \
	struct xlist_head *head, *next; \
\
	list_magic_check(&(l), "free_list"); \
	do_not_optimize_here_gcc_2_7(&l); \
	foreach (head, (l)) do_not_optimize_here_gcc_3_x(head); /* AA */ \
	foreachback (head, (l)) do_not_optimize_here_gcc_3_x(head); /* AA */ \
	foreachsafe (head, next, l) { \
		del_from_list(head); \
		mem_free(head); \
	} \
	do_not_optimize_here_gcc_2_7(&l); \
} while (0)

static inline int
list_size(struct list_head *list)
{
	struct list_head *item;
	int size = 0;

	foreach (item, *list)
		size++;

	return size;
}

#define move_to_top_of_list(list, item) do { \
	if ((item) != (list).next) { \
		del_from_list(item); \
		add_to_list(list, item); \
	} \
} while (0)


#endif /* EL__UTIL_LISTS_H */
