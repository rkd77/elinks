#ifndef EL__UTIL_MEMLIST_H
#define EL__UTIL_MEMLIST_H

struct memory_list {
	int n;
	void *p[1];
};

#undef DEBUG_MEMLIST
#ifdef CONFIG_DEBUG
#define DEBUG_MEMLIST
#endif

#if defined(DEBUG_MEMLIST) && defined(HAVE_VARIADIC_MACROS)
struct memory_list *debug_getml(unsigned char *file, int line, void *p, ...);
void debug_add_to_ml(unsigned char *file, int line, struct memory_list **ml, ...);
#define getml(...) debug_getml(__FILE__, __LINE__, __VA_ARGS__)
#define add_to_ml(...) debug_add_to_ml(__FILE__, __LINE__, __VA_ARGS__)
#else
struct memory_list *getml(void *p, ...);
void add_to_ml(struct memory_list **ml, ...);
#endif

#ifdef DEBUG_MEMLIST
void debug_add_one_to_ml(unsigned char *file, int line, struct memory_list **ml, void *p);
#define add_one_to_ml(ml, p) debug_add_one_to_ml(__FILE__, __LINE__, ml, p)
#else
void add_one_to_ml(struct memory_list **ml, void *p);
#endif

void freeml(struct memory_list *);

#endif
