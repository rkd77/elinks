/* Some ELinks' auxiliary routines (ELinks<->gettext support) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "intl/gettext/gettextP.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* See libintl.h for comments.  */
int current_charset = -1;

/* This is a language lookup table. Indexed by code. */
/* Update this everytime you add a new translation. */
/* TODO: Try to autogenerate it somehow. Maybe just a complete table? Then we
 * will anyway need a table of real translations. */
struct language languages[] = {
	{N_("System"), "system"},
	{N_("English"), "en"},

	{N_("Afrikaans"), "af"},
	{N_("Belarusian"), "be"},
	{N_("Brazilian Portuguese"), "pt-BR"},
	{N_("Bulgarian"), "bg"},
	{N_("Catalan"), "ca"},
	{N_("Croatian"), "hr"},
	{N_("Czech"), "cs"},
	{N_("Danish"), "da"},
	{N_("Dutch"), "nl"},
	{N_("Estonian"), "et"},
	{N_("Finnish"), "fi"},
	{N_("French"), "fr"},
	{N_("Galician"), "gl"},
	{N_("German"), "de"},
	{N_("Greek"), "el"},
	{N_("Hungarian"), "hu"},
	{N_("Icelandic"), "is"},
	{N_("Indonesian"), "id"},
	{N_("Italian"), "it"},
	{N_("Lithuanian"), "lt"},
	{N_("Norwegian"), "no"},
	{N_("Polish"), "pl"},
	{N_("Portuguese"), "pt"},
	{N_("Romanian"), "ro"},
	{N_("Russian"), "ru"},
	{N_("Serbian"), "sr"},
	{N_("Slovak"), "sk"},
	{N_("Spanish"), "es"},
	{N_("Swedish"), "sv"},
	{N_("Turkish"), "tr"},
	{N_("Ukrainian"), "uk"},

	{NULL, NULL},
};

/* XXX: In fact this is _NOT_ a real ISO639 code but RFC3066 code (as we're
 * supposed to use that one when sending language tags through HTTP/1.1) and
 * that one consists basically from ISO639[-ISO3166].  This is important for
 * ie. pt vs pt-BR. */
/* TODO: We should reflect this in name of this function and of the tag. On the
 * other side, it's ISO639 for gettext as well etc. So what?  --pasky */

int
iso639_to_language(unsigned char *iso639)
{
	unsigned char *l = stracpy(iso639);
	unsigned char *p;
	int i, ll;

	if (!l)
		return 1;

	/* The environment variable transformation. */

	p = strchr(l, '.');
	if (p)
		*p = '\0';

	p = strchr(l, '_');
	if (p)
		*p = '-';
	else
		p = strchr(l, '-');

	/* Exact match. */

	for (i = 0; languages[i].name; i++) {
		if (strcmp(languages[i].iso639, l))
			continue;
		mem_free(l);
		return i;
	}

	/* Base language match. */

	if (p) {
		*p = '\0';
		for (i = 0; languages[i].name; i++) {
			if (strcmp(languages[i].iso639, l))
				continue;
			mem_free(l);
			return i;
		}
	}

	/* Any dialect match. */

	ll = strlen(l);
	for (i = 0; languages[i].name; i++) {
		int il = strcspn(languages[i].iso639, "-");

		if (strncmp(languages[i].iso639, l, il > ll ? ll : il))
			continue;
		mem_free(l);
		return i;
	}

	/* Default to english. */

	mem_free(l);
	return 1;
}

int system_language = 0;

unsigned char *
language_to_iso639(int language)
{
	/* Language is "system", we need to extract the index from
	 * the environment */
	if (language == 0) {
		return system_language ?
			languages[system_language].iso639 :
			languages[get_system_language_index()].iso639;
	}

	return languages[language].iso639;
}

int
name_to_language(const unsigned char *name)
{
	int i;

	for (i = 0; languages[i].name; i++) {
		if (c_strcasecmp(languages[i].name, name))
			continue;
		return i;
	}
	return 1;
}

unsigned char *
language_to_name(int language)
{
	return languages[language].name;
}

int
get_system_language_index(void)
{
	unsigned char *l;

	/* At this point current_language must be "system" yet. */
	l = getenv("LANGUAGE");
	if (!l)
		l = getenv("LC_ALL");
	if (!l)
		l = getenv("LC_MESSAGES");
	if (!l)
		l = getenv("LANG");

	return (l) ? iso639_to_language(l) : 1;
}

int current_language = 0;

void
set_language(int language)
{
	unsigned char *p;

	if (!system_language)
		system_language = get_system_language_index();

	if (language == current_language) {
		/* Nothing to do. */
		return;
	}

	current_language = language;

	if (!language)
		language = system_language;

	if (!LANGUAGE) {
		/* We never free() this, purely intentionally. */
		LANGUAGE = malloc(256);
	}
	strcpy(LANGUAGE, language_to_iso639(language));
	p = strchr(LANGUAGE, '-');
	if (p)
		*p = '_';

	/* Propagate the change to gettext. From the info manual. */
	{
		extern int _nl_msg_cat_cntr;

		_nl_msg_cat_cntr++;
	}
}
