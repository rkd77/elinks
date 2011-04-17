/* HTML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "bookmarks/bookmarks.h"
#include "config/options.h"
#include "config/kbdbind.h"
#include "document/html/frames.h"
#include "document/html/parser/link.h"
#include "document/html/parser/parse.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "globhist/globhist.h"
#include "mime/mime.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


void
html_a(struct html_context *html_context, unsigned char *a,
       unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *href;

	href = get_url_val(a, "href", html_context->doc_cp);
	if (href) {
		unsigned char *target;

		mem_free_set(&format.link,
			     join_urls(html_context->base_href,
				       trim_chars(href, ' ', 0)));

		mem_free(href);

		target = get_target(html_context->options, a);
		if (target) {
			mem_free_set(&format.target, target);
		} else {
			mem_free_set(&format.target, stracpy(html_context->base_target));
		}

		if (0) {
			; /* Shut up compiler */
#ifdef CONFIG_GLOBHIST
		} else if (get_global_history_item(format.link)) {
			format.style.fg = format.vlink;
			html_top->pseudo_class &= ~ELEMENT_LINK;
			html_top->pseudo_class |= ELEMENT_VISITED;
#endif
#ifdef CONFIG_BOOKMARKS
		} else if (get_bookmark(format.link)) {
			format.style.fg = format.bookmark_link;
			html_top->pseudo_class &= ~ELEMENT_VISITED;
			/* XXX: Really set ELEMENT_LINK? --pasky */
			html_top->pseudo_class |= ELEMENT_LINK;
#endif
		} else {
			format.style.fg = format.clink;
			html_top->pseudo_class &= ~ELEMENT_VISITED;
			html_top->pseudo_class |= ELEMENT_LINK;
		}

		mem_free_set(&format.title,
		             get_attr_val(a, "title", html_context->doc_cp));

		html_focusable(html_context, a);

	} else {
		pop_html_element(html_context);
	}

	set_fragment_identifier(html_context, a, "name");
}

/* Returns an allocated string made after @label
 * but limited to @max_len length, by truncating
 * the middle of @label string, which is replaced
 * by an asterisk ('*').
 * If @max_len < 0 it returns NULL.
 * If @max_len == 0 it returns an unmodified copy
 * of @label string.
 * In either case, it may return NULL if a memory
 * allocation failure occurs.
 * Example:
 * truncate_label("some_string", 5) => "so*ng" */
static unsigned char *
truncate_label(unsigned char *label, int max_len)
{
	unsigned char *new_label;
	int len = strlen(label);
	int left_part_len;
	int right_part_len;

	if (max_len < 0) return NULL;
	if (max_len == 0 || len <= max_len)
		return stracpy(label);

	right_part_len = left_part_len = max_len / 2;

	if (left_part_len + right_part_len + 1 > max_len)
		right_part_len--;

	new_label = mem_alloc(max_len + 1);
	if (!new_label) return NULL;

	if (left_part_len)
		memcpy(new_label, label, left_part_len);

	new_label[left_part_len] = '*';

	if (right_part_len)
		memcpy(new_label + left_part_len + 1,
		       label + len - right_part_len, right_part_len);

	new_label[max_len] = '\0';

	return new_label;
}

/* Get image filename from its src attribute. */
static unsigned char *
get_image_filename_from_src(int max_len, unsigned char *src)
{
	unsigned char *text = NULL;
	unsigned char *start, *filename;
	int len;

	if (!src) return NULL;
	/* We can display image as [foo.gif]. */

	len = strcspn(src, "?");

	for (start = src + len; start > src; start--)
		if (dir_sep(start[-1])) {
			break;
		}

	len -= start - src;

	filename = memacpy(start, len);
	if (filename) {
		/* XXX: Due to a compatibility alias (added: 2004-12-15 in
		 * 0.10pre3.CVS for document.browse.images.file_tags) this can
		 * return a negative @max_len. */
		text = truncate_label(filename, max_len);
		mem_free(filename);
	}

	return text;
}


/* Returns an allocated string containing formatted @label. */
static unsigned char *
get_image_label(int max_len, unsigned char *label)
{
	unsigned char *formatted_label;

	if (!label) return NULL;

	formatted_label = truncate_label(label, max_len);
	mem_free(label);

	return formatted_label;
}

static void
put_image_label(unsigned char *a, unsigned char *label,
                struct html_context *html_context)
{
	color_T saved_foreground;
	enum text_style_format saved_attr;

	/* This is not 100% appropriate for <img>, but well, accepting
	 * accesskey and tabindex near <img> is just our little
	 * extension to the standard. After all, it makes sense. */
	html_focusable(html_context, a);

	saved_foreground = format.style.fg;
	saved_attr = format.style.attr;
	format.style.fg = format.image_link;
	format.style.attr |= AT_NO_ENTITIES;
	put_chrs(html_context, label, strlen(label));
	format.style.fg = saved_foreground;
	format.style.attr = saved_attr;
}

static void
html_img_do(unsigned char *a, unsigned char *object_src,
            struct html_context *html_context)
{
	int ismap, usemap = 0;
	int add_brackets = 0;
	unsigned char *src = NULL;
	unsigned char *label = NULL;
	unsigned char *usemap_attr;
	struct document_options *options = html_context->options;
	int display_style = options->image_link.display_style;

	/* Note about display_style:
	 * 0     means always display IMG
	 * 1     means always display filename
	 * 2     means display alt/title attribute if possible, IMG if not
	 * 3     means display alt/title attribute if possible, filename if not */

	usemap_attr = get_attr_val(a, "usemap", html_context->doc_cp);
	if (usemap_attr) {
		unsigned char *joined_urls = join_urls(html_context->base_href,
						       usemap_attr);
		unsigned char *map_url;

		mem_free(usemap_attr);
		if (!joined_urls) return;
		map_url = straconcat("MAP@", joined_urls,
				     (unsigned char *) NULL);
		mem_free(joined_urls);
		if (!map_url) return;

		html_stack_dup(html_context, ELEMENT_KILLABLE);
		mem_free_set(&format.link, map_url);
		format.form = NULL;
		format.style.attr |= AT_BOLD;
		usemap = 1;
 	}

	ismap = format.link
	        && has_attr(a, "ismap", html_context->doc_cp)
	        && !usemap;

	if (display_style == 2 || display_style == 3) {
		label = get_attr_val(a, "alt", html_context->doc_cp);
		if (!label)
			label = get_attr_val(a, "title", html_context->doc_cp);

		/* Little hack to preserve rendering of [   ], in directory listings,
		 * but we still want to drop extra spaces in alt or title attribute
		 * to limit display width on certain websites. --Zas */
		if (label && strlen(label) > 5) clr_spaces(label);
	}

	src = null_or_stracpy(object_src);
	if (!src) src = get_url_val(a, "src", html_context->doc_cp);
	if (!src) src = get_url_val(a, "dynsrc", html_context->doc_cp);

	/* If we have no label yet (no title or alt), so
	 * just use default ones, or image filename. */
	if (!label || !*label) {
		mem_free_set(&label, NULL);
		/* Do we want to display images with no alt/title and with no
		 * link on them ?
		 * If not, just exit now. */
		if (!options->images && !format.link) {
			mem_free_if(src);
			if (usemap) pop_html_element(html_context);
			return;
		}

		add_brackets = 1;

		if (usemap) {
			label = stracpy("USEMAP");
		} else if (ismap) {
			label = stracpy("ISMAP");
		} else {
			if (display_style == 3)
				label = get_image_filename_from_src(options->image_link.filename_maxlen, src);
		}

	} else {
		label = get_image_label(options->image_link.label_maxlen, label);
	}

	if (!label || !*label) {
		mem_free_set(&label, NULL);
		add_brackets = 1;
		if (display_style == 1)
			label = get_image_filename_from_src(options->image_link.filename_maxlen, src);
		if (!label || !*label)
			mem_free_set(&label, stracpy("IMG"));
	}

	mem_free_set(&format.image, NULL);
	mem_free_set(&format.title, NULL);

	if (label) {
		int img_link_tag = options->image_link.tagging;

		if (img_link_tag && (img_link_tag == 2 || add_brackets)) {
			unsigned char *img_link_prefix = options->image_link.prefix;
			unsigned char *img_link_suffix = options->image_link.suffix;
			unsigned char *new_label = straconcat(img_link_prefix, label, img_link_suffix, (unsigned char *) NULL);

			if (new_label) mem_free_set(&label, new_label);
		}

		if (!options->image_link.show_any_as_links) {
			put_image_label(a, label, html_context);

		} else {
			if (src) {
				format.image = join_urls(html_context->base_href, src);
			}

			format.title = get_attr_val(a, "title", html_context->doc_cp);

			if (ismap) {
				unsigned char *new_link;

				html_stack_dup(html_context, ELEMENT_KILLABLE);
				new_link = straconcat(format.link, "?0,0", (unsigned char *) NULL);
				if (new_link)
					mem_free_set(&format.link, new_link);
			}

			put_image_label(a, label, html_context);

			if (ismap) pop_html_element(html_context);
			mem_free_set(&format.image, NULL);
			mem_free_set(&format.title, NULL);
		}

		mem_free(label);
	}

	mem_free_if(src);
	if (usemap) pop_html_element(html_context);
}

void
html_img(struct html_context *html_context, unsigned char *a,
         unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	html_img_do(a, NULL, html_context);
}

/* prefix can have entities in it, but linkname cannot.  */
void
put_link_line(unsigned char *prefix, unsigned char *linkname,
	      unsigned char *link, unsigned char *target,
	      struct html_context *html_context)
{
	html_context->has_link_lines = 1;
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	ln_break(html_context, 1);
	mem_free_set(&format.link, NULL);
	mem_free_set(&format.target, NULL);
	mem_free_set(&format.title, NULL);
	format.form = NULL;
	put_chrs(html_context, prefix, strlen(prefix));
	format.link = join_urls(html_context->base_href, link);
	format.target = stracpy(target);
	format.style.fg = format.clink;
	/* linkname typically comes from get_attr_val, which
	 * has already expanded character entity references.
	 * Tell put_chrs not to expand them again.  */
	format.style.attr |= AT_NO_ENTITIES;
	put_chrs(html_context, linkname, strlen(linkname));
	ln_break(html_context, 1);
	pop_html_element(html_context);
}


void
html_applet(struct html_context *html_context, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *code, *alt;

	code = get_url_val(a, "code", html_context->doc_cp);
	if (!code) return;

	alt = get_attr_val(a, "alt", html_context->doc_cp);

	html_focusable(html_context, a);

	if (alt && *alt) {
		put_link_line("Applet: ", alt, code,
			      html_context->options->framename, html_context);
	} else {
		put_link_line("", "Applet", code,
			      html_context->options->framename, html_context);
	}

	mem_free_if(alt);
	mem_free(code);
}

static void
html_iframe_do(unsigned char *a, unsigned char *object_src,
               struct html_context *html_context)
{
	unsigned char *name, *url = NULL;

	url = null_or_stracpy(object_src);
	if (!url) url = get_url_val(a, "src", html_context->doc_cp);
	if (!url) return;

	name = get_attr_val(a, "name", html_context->doc_cp);
	if (!name) name = get_attr_val(a, "id", html_context->doc_cp);
	if (!name) name = stracpy("");
	if (!name) {
		mem_free(url);
		return;
	}

	html_focusable(html_context, a);

	if (*name) {
		put_link_line("IFrame: ", name, url,
			      html_context->options->framename, html_context);
	} else {
		put_link_line("", "IFrame", url,
			      html_context->options->framename, html_context);
	}

	mem_free(name);
	mem_free(url);
}

void
html_iframe(struct html_context *html_context, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	html_iframe_do(a, NULL, html_context);
}

void
html_object(struct html_context *html_context, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *type, *url;

	/* This is just some dirty wrapper. We emulate various things through
	 * this, which is anyway in the spirit of <object> element, unifying
	 * <img> and <iframe> etc. */

	url = get_url_val(a, "data", html_context->doc_cp);
	if (!url) url = get_url_val(a, "codebase", html_context->doc_cp);
	if (!url) return;

	type = get_attr_val(a, "type", html_context->doc_cp);
	if (!type) { mem_free(url); return; }

	if (!c_strncasecmp(type, "text/", 5)) {
		/* We will just emulate <iframe>. */
		html_iframe_do(a, url, html_context);
		html_skip(html_context, a);

	} else if (!c_strncasecmp(type, "image/", 6)) {
		/* <img> emulation. */
		/* TODO: Use the enclosed text as 'alt' attribute. */
		html_img_do(a, url, html_context);
	} else {
		unsigned char *name;

		name = get_attr_val(a, "standby", html_context->doc_cp);

		html_focusable(html_context, a);

		if (name && *name) {
			put_link_line("Object: ", name, url,
			              html_context->options->framename,
				      html_context);
		} else {
			put_link_line("Object: ", type, url,
			              html_context->options->framename,
			              html_context);
		}

		mem_free_if(name);
	}

	mem_free(type);
	mem_free(url);
}

void
html_embed(struct html_context *html_context, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *type, *extension;
	unsigned char *object_src;

	/* This is just some dirty wrapper. We emulate various things through
	 * this, which is anyway in the spirit of <object> element, unifying
	 * <img> and <iframe> etc. */

	object_src = get_url_val(a, "src", html_context->doc_cp);
	if (!object_src || !*object_src) {
		mem_free_set(&object_src, NULL);
		return;
	}

	/* If there is no extension we want to get the default mime/type
	 * anyway? */
	extension = strrchr(object_src, '.');
	if (!extension) extension = object_src;

	type = get_extension_content_type(extension);
	if (type && !c_strncasecmp(type, "image/", 6)) {
		html_img_do(a, object_src, html_context);
	} else {
		/* We will just emulate <iframe>. */
		html_iframe_do(a, object_src, html_context);
	}

	mem_free_if(type);
	mem_free_set(&object_src, NULL);
}



/* Link types:

Alternate
	Designates substitute versions for the document in which the link
	occurs. When used together with the lang attribute, it implies a
	translated version of the document. When used together with the
	media attribute, it implies a version designed for a different
	medium (or media).

Stylesheet
	Refers to an external style sheet. See the section on external style
	sheets for details. This is used together with the link type
	"Alternate" for user-selectable alternate style sheets.

Start
	Refers to the first document in a collection of documents. This link
	type tells search engines which document is considered by the author
	to be the starting point of the collection.

Next
	Refers to the next document in a linear sequence of documents. User
	agents may choose to preload the "next" document, to reduce the
	perceived load time.

Prev
	Refers to the previous document in an ordered series of documents.
	Some user agents also support the synonym "Previous".

Contents
	Refers to a document serving as a table of contents.
	Some user agents also support the synonym ToC (from "Table of Contents").

Index
	Refers to a document providing an index for the current document.

Glossary
	Refers to a document providing a glossary of terms that pertain to the
	current document.

Copyright
	Refers to a copyright statement for the current document.

Chapter
        Refers to a document serving as a chapter in a collection of documents.

Section
	Refers to a document serving as a section in a collection of documents.

Subsection
	Refers to a document serving as a subsection in a collection of
	documents.

Appendix
	Refers to a document serving as an appendix in a collection of
	documents.

Help
	Refers to a document offering help (more information, links to other
	sources information, etc.)

Bookmark
	Refers to a bookmark. A bookmark is a link to a key entry point
	within an extended document. The title attribute may be used, for
	example, to label the bookmark. Note that several bookmarks may be
	defined in each document.

Some more were added, like top. --Zas */

enum hlink_type {
	LT_UNKNOWN = 0,
	LT_START,
	LT_PARENT,
	LT_NEXT,
	LT_PREV,
	LT_CONTENTS,
	LT_INDEX,
	LT_GLOSSARY,
	LT_CHAPTER,
	LT_SECTION,
	LT_SUBSECTION,
	LT_APPENDIX,
	LT_HELP,
	LT_SEARCH,
	LT_BOOKMARK,
	LT_COPYRIGHT,
	LT_AUTHOR,
	LT_ICON,
	LT_ALTERNATE,
	LT_ALTERNATE_LANG,
	LT_ALTERNATE_MEDIA,
	LT_ALTERNATE_STYLESHEET,
	LT_STYLESHEET,
};

enum hlink_direction {
	LD_UNKNOWN = 0,
	LD_REV,
	LD_REL,
};

struct hlink {
	enum hlink_type type;
	enum hlink_direction direction;
	unsigned char *content_type;
	unsigned char *media;
	unsigned char *href;
	unsigned char *hreflang;
	unsigned char *title;
	unsigned char *lang;
	unsigned char *name;
/* Not implemented yet.
	unsigned char *charset;
	unsigned char *target;
	unsigned char *id;
	unsigned char *class;
	unsigned char *dir;
*/
};

struct lt_default_name {
	enum hlink_type type;
	unsigned char *str;
};

/* TODO: i18n */
/* XXX: Keep the (really really ;) default name first */
static struct lt_default_name lt_names[] = {
	{ LT_START, "start" },
	{ LT_START, "top" },
	{ LT_START, "home" },
	{ LT_PARENT, "parent" },
	{ LT_PARENT, "up" },
	{ LT_NEXT, "next" },
	{ LT_PREV, "previous" },
	{ LT_PREV, "prev" },
	{ LT_CONTENTS, "contents" },
	{ LT_CONTENTS, "toc" },
	{ LT_INDEX, "index" },
	{ LT_GLOSSARY, "glossary" },
	{ LT_CHAPTER, "chapter" },
	{ LT_SECTION, "section" },
	{ LT_SUBSECTION, "subsection" },
	{ LT_SUBSECTION, "child" },
	{ LT_SUBSECTION, "sibling" },
	{ LT_APPENDIX, "appendix" },
	{ LT_HELP, "help" },
	{ LT_SEARCH, "search" },
	{ LT_BOOKMARK, "bookmark" },
	{ LT_ALTERNATE_LANG, "alt. language" },
	{ LT_ALTERNATE_MEDIA, "alt. media" },
	{ LT_ALTERNATE_STYLESHEET, "alt. stylesheet" },
	{ LT_STYLESHEET, "stylesheet" },
	{ LT_ALTERNATE, "alternate" },
	{ LT_COPYRIGHT, "copyright" },
	{ LT_AUTHOR, "author" },
	{ LT_AUTHOR, "made" },
	{ LT_AUTHOR, "owner" },
	{ LT_ICON, "icon" },
	{ LT_UNKNOWN, NULL }
};

/* Search for default name for this link according to its type. */
static unsigned char *
get_lt_default_name(struct hlink *link)
{
	struct lt_default_name *entry = lt_names;

	assert(link);

	while (entry && entry->str) {
		if (entry->type == link->type) return entry->str;
		entry++;
	}

	return "unknown";
}

static void
html_link_clear(struct hlink *link)
{
	assert(link);

	mem_free_if(link->content_type);
	mem_free_if(link->media);
	mem_free_if(link->href);
	mem_free_if(link->hreflang);
	mem_free_if(link->title);
	mem_free_if(link->lang);
	mem_free_if(link->name);

	memset(link, 0, sizeof(*link));
}

/* Parse a link and return results in @link.
 * It tries to identify known types. */
static int
html_link_parse(struct html_context *html_context, unsigned char *a,
                struct hlink *link)
{
	int i;

	assert(a && link);
	memset(link, 0, sizeof(*link));

	link->href = get_url_val(a, "href", html_context->doc_cp);
	if (!link->href) return 0;

	link->lang = get_attr_val(a, "lang", html_context->doc_cp);
	link->hreflang = get_attr_val(a, "hreflang", html_context->doc_cp);
	link->title = get_attr_val(a, "title", html_context->doc_cp);
	link->content_type = get_attr_val(a, "type", html_context->doc_cp);
	link->media = get_attr_val(a, "media", html_context->doc_cp);

	link->name = get_attr_val(a, "rel", html_context->doc_cp);
	if (link->name) {
		link->direction = LD_REL;
	} else {
		link->name = get_attr_val(a, "rev", html_context->doc_cp);
		if (link->name) link->direction = LD_REV;
	}

	if (!link->name) return 1;

	/* TODO: fastfind */
	for (i = 0; lt_names[i].str; i++)
		if (!c_strcasecmp(link->name, lt_names[i].str)) {
			link->type = lt_names[i].type;
			return 1;
		}

	if (c_strcasestr(link->name, "icon") ||
	   (link->content_type && c_strcasestr(link->content_type, "icon"))) {
		link->type = LT_ICON;

	} else if (c_strcasestr(link->name, "alternate")) {
		link->type = LT_ALTERNATE;
		if (link->lang)
			link->type = LT_ALTERNATE_LANG;
		else if (c_strcasestr(link->name, "stylesheet") ||
			 (link->content_type && c_strcasestr(link->content_type, "css")))
			link->type = LT_ALTERNATE_STYLESHEET;
		else if (link->media)
			link->type = LT_ALTERNATE_MEDIA;

	} else if (link->content_type && c_strcasestr(link->content_type, "css")) {
		link->type = LT_STYLESHEET;
	}

	return 1;
}

void
html_link(struct html_context *html_context, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	int link_display = html_context->options->meta_link_display;
	unsigned char *name;
	struct hlink link;
	struct string text;
	int name_neq_title = 0;
	int first = 1;

#ifndef CONFIG_CSS
	if (!link_display) return;
#endif
	if (!html_link_parse(html_context, a, &link)) return;
	if (!link.href) goto free_and_return;

#ifdef CONFIG_CSS
	if (link.type == LT_STYLESHEET) {
		int len = strlen(link.href);

		import_css_stylesheet(&html_context->css_styles,
				      html_context->base_href, link.href, len);
	}

	if (!link_display) goto free_and_return;
#endif

	/* Ignore few annoying links.. */
	if (link_display < 5 &&
	    (link.type == LT_ICON ||
	     link.type == LT_AUTHOR ||
	     link.type == LT_STYLESHEET ||
	     link.type == LT_ALTERNATE_STYLESHEET)) goto free_and_return;

	if (!link.name || link.type != LT_UNKNOWN)
		/* Give preference to our default names for known types. */
		name = get_lt_default_name(&link);
	else
		name = link.name;

	if (!name) goto free_and_return;
	if (!init_string(&text)) goto free_and_return;

	html_focusable(html_context, a);

	if (link.title) {
		add_to_string(&text, link.title);
		name_neq_title = strcmp(link.title, name);
	} else
		add_to_string(&text, name);

	if (link_display == 1) goto put_link_line;	/* Only title */

#define APPEND(what) do { \
		add_to_string(&text, first ? " (" : ", "); \
		add_to_string(&text, (what)); \
		first = 0; \
	} while (0)

	if (name_neq_title) {
		APPEND(name);
	}

	if (link_display >= 3 && link.hreflang) {
		APPEND(link.hreflang);
	}

	if (link_display >= 4 && link.content_type) {
		APPEND(link.content_type);
	}

	if (link.lang && link.type == LT_ALTERNATE_LANG &&
	    (link_display < 3 || (link.hreflang &&
				  c_strcasecmp(link.hreflang, link.lang)))) {
		APPEND(link.lang);
	}

	if (link.media) {
		APPEND(link.media);
	}

#undef APPEND

	if (!first) add_char_to_string(&text, ')');

put_link_line:
	{
		unsigned char *prefix = (link.direction == LD_REL)
					? "Link: " : "Reverse link: ";
		unsigned char *link_name = (text.length)
					   ? text.source : name;

		put_link_line(prefix, link_name, link.href,
			      html_context->base_target, html_context);

		if (text.source) done_string(&text);
	}

free_and_return:
	html_link_clear(&link);
}
