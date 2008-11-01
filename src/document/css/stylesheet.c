/** CSS stylesheet handling
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/stylesheet.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


/* You can find some mysterious functions commented out here. I planned to use
 * them for various smart things (well they all report to
 * merge_css_stylesheets()), but it turns out it makes no sense to merge
 * stylesheets now (and maybe it won't in the future neither...). But maybe you
 * will find them useful at some time, so... Dunno. --pasky */


struct css_selector *
find_css_selector(struct css_selector_set *sels,
                  enum css_selector_type type,
                  enum css_selector_relation rel,
                  const unsigned char *name, int namelen)
{
	struct css_selector *selector;

	assert(sels && name);

	foreach_css_selector (selector, sels) {
		if (type != selector->type || rel != selector->relation)
			continue;
		if (c_strlcasecmp(name, namelen, selector->name, -1))
			continue;
		return selector;
	}

	return NULL;
}

struct css_selector *
init_css_selector(struct css_selector_set *sels,
                  enum css_selector_type type,
                  enum css_selector_relation relation,
                  const unsigned char *name, int namelen)
{
	struct css_selector *selector;

	selector = mem_calloc(1, sizeof(*selector));
	if (!selector) return NULL;

	selector->relation = relation;
	init_css_selector_set(&selector->leaves);

	selector->type = type;
	init_list(selector->properties);

	if (name) {
		if (namelen < 0)
			namelen = strlen(name);
		selector->name = memacpy(name, namelen);
		if (!selector->name) {
			done_css_selector_set(&selector->leaves);
			mem_free(selector);
			return NULL;
		}
		set_mem_comment(selector, name, namelen);
	}

	if (sels) {
		add_css_selector_to_set(selector, sels);
	}

	return selector;
}

void
set_css_selector_relation(struct css_selector *selector,
			  enum css_selector_relation relation)
{
	/* Changing the relation after the selector is in a set might require
	 * setting css_relation_set.may_contain_rel_ancestor_or_parent,
	 * but we don't have a pointer to the set here.  */
	assert(!css_selector_is_in_set(selector));
	selector->relation = relation;
}

struct css_selector *
get_css_selector(struct css_selector_set *sels,
                 enum css_selector_type type,
                 enum css_selector_relation rel,
                 const unsigned char *name, int namelen)
{
	struct css_selector *selector = NULL;

	if (sels && name && namelen) {
		selector = find_css_selector(sels, type, rel, name, namelen);
		if (selector)
			return selector;
	}

	return init_css_selector(sels, type, rel, name, namelen);
}

static struct css_selector *
copy_css_selector(struct css_stylesheet *css, struct css_selector *orig)
{
	struct css_selector *copy;

	assert(css && orig);
	assert(orig->relation == CSR_ROOT);

	copy = init_css_selector(&css->selectors, orig->type, CSR_ROOT,
	                         orig->name, strlen(orig->name));
	if (!copy)
		return NULL;

	return copy;
}

static void
add_selector_property(struct css_selector *selector, struct css_property *prop)
{
	struct css_property *newprop = mem_alloc(sizeof(*newprop));

	if (newprop) {
		copy_struct(newprop, prop);
		add_to_list(selector->properties, newprop);
	}
}

void
add_selector_properties(struct css_selector *selector,
                        LIST_OF(struct css_property) *properties)
{
	struct css_property *prop;

	foreach (prop, *properties) {
		add_selector_property(selector, prop);
	}
}

static struct css_selector *
clone_css_selector(struct css_stylesheet *css, struct css_selector *orig)
{
	struct css_selector *copy;

	assert(css && orig);

	copy = copy_css_selector(css, orig);
	if (!copy)
		return NULL;
	add_selector_properties(copy, &orig->properties);
	return copy;
}

void
merge_css_selectors(struct css_selector *sel1, struct css_selector *sel2)
{
	struct css_property *prop;

	foreach (prop, sel2->properties) {
		struct css_property *origprop;

		foreach (origprop, sel1->properties)
			if (origprop->type == prop->type) {
				del_from_list(origprop);
				mem_free(origprop);
				break;
			}

		/* Not there yet, let's add it. */
		add_selector_property(sel1, prop);
	}
}

void
done_css_selector(struct css_selector *selector)
{
	done_css_selector_set(&selector->leaves);

	if (css_selector_is_in_set(selector))
		del_css_selector_from_set(selector);
	free_list(selector->properties);
	mem_free_if(selector->name);
	mem_free(selector);
}

void
init_css_selector_set(struct css_selector_set *set)
{
	set->may_contain_rel_ancestor_or_parent = 0;
	init_list(set->list);
}

void
done_css_selector_set(struct css_selector_set *set)
{
	while (!css_selector_set_empty(set)) {
		done_css_selector(css_selector_set_front(set));
	}
}

void
add_css_selector_to_set(struct css_selector *selector,
			struct css_selector_set *set)
{
	assert(!css_selector_is_in_set(selector));

	add_to_list(set->list, selector);
	if (selector->relation == CSR_ANCESTOR
	    || selector->relation == CSR_PARENT)
		set->may_contain_rel_ancestor_or_parent = 1;
}

void
del_css_selector_from_set(struct css_selector *selector)
{
	del_from_list(selector);
	selector->next = NULL;
	selector->prev = NULL;
}

#ifdef DEBUG_CSS
void
dump_css_selector_tree_iter(struct css_selector_set *sels, int level)
{
	struct css_selector *sel;

	foreach_css_selector (sel, sels) {
		struct css_property *prop;

		fprintf(stderr, "%*s +- [%s] type %d rel %d props",
		        level * 4, " ",
		        sel->name, sel->type, sel->relation);
		foreach (prop, sel->properties) {
			fprintf(stderr, " [%d]", prop->type);
		}
		fprintf(stderr, "\n");
		dump_css_selector_tree_iter(&sel->leaves, level + 1);
	}
}

void
dump_css_selector_tree(struct css_selector_set *sels)
{
	dump_css_selector_tree_iter(sels, 0);
}
#endif


#if 0 /* used only by clone_css_stylesheet */
struct css_stylesheet *
init_css_stylesheet(css_stylesheet_importer_T importer, void *import_data)
{
	struct css_stylesheet *css;

	css = mem_calloc(1, sizeof(*css));
	if (!css)
		return NULL;
	css->import = importer;
	css->import_data = import_data;
	init_css_selector_set(&css->selectors);
	return css;
}
#endif

void
mirror_css_stylesheet(struct css_stylesheet *css1, struct css_stylesheet *css2)
{
	struct css_selector *selector;

	foreach_css_selector (selector, &css1->selectors) {
		clone_css_selector(css2, selector);
	}
}

#if 0
struct css_stylesheet *
clone_css_stylesheet(struct css_stylesheet *orig)
{
	struct css_stylesheet *copy;
	struct css_selector *selector;

	copy = init_css_stylesheet(orig->import, orig->import_data);
	if (!copy)
		return NULL;
	mirror_css_stylesheet(orig, copy);
	return copy;
}

void
merge_css_stylesheets(struct css_stylesheet *css1,
		      struct css_stylesheet *css2)
{
	struct css_selector *selector;

	assert(css1 && css2);

	/* This is 100% evil. And gonna be a huge bottleneck. Essentially
	 * O(N^2) where we could be much smarter (ie. sort it once and then
	 * always be O(N)). */

	foreach_css_selector (selector, &css2->selectors) {
		struct css_selector *origsel;

		origsel = find_css_selector(&css1->selectors, selector->name,
					    strlen(selector->name));
		if (!origsel) {
			clone_css_selector(css1, selector);
		} else {
			merge_css_selectors(origsel, selector);
		}
	}
}
#endif

void
done_css_stylesheet(struct css_stylesheet *css)
{
	done_css_selector_set(&css->selectors);
}
