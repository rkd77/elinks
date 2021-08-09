#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/libintl.h"

/* The number of the charset to which the "elinks" domain was last
 * bound with bind_textdomain_codeset(), or -1 if none yet.  This
 * cannot be a static variable in _(), because then it would get
 * duplicated in every translation unit, even though the actual
 * binding is global.  */
int current_charset = -1;

/* This is a language lookup table. Indexed by code. */
/* Update this everytime you add a new translation. */
/* TODO: Try to autogenerate it somehow. Maybe just a complete table? Then we
 * will anyway need a table of real translations. */
struct language languages[] = {
	{N_("System"), "system"},
	{N_("English"), "en_GB.UTF-8"},

	{N_("Afrikaans"), "af_ZA.UTF-8"},
	{N_("Belarusian"), "be_BY.UTF-8"},
	{N_("Brazilian Portuguese"), "pt_BR.UTF-8"},
	{N_("Bulgarian"), "bg_BG.UTF-8"},
	{N_("Catalan"), "ca_ES.UTF-8"},
	{N_("Croatian"), "hr_HR.UTF-8"},
	{N_("Czech"), "cs_CZ.UTF-8"},
	{N_("Danish"), "da_DK.UTF-8"},
	{N_("Dutch"), "nl_NL.UTF-8"},
	{N_("Estonian"), "et_EE.UTF-8"},
	{N_("Finnish"), "fi_FI.UTF-8"},
	{N_("French"), "fr_FR.UTF-8"},
	{N_("Galician"), "gl_ES.UTF-8"},
	{N_("German"), "de_DE.UTF-8"},
	{N_("Greek"), "el_GR.UTF-8"},
	{N_("Hungarian"), "hu_HU.UTF-8"},
	{N_("Icelandic"), "is_IS.UTF-8"},
	{N_("Indonesian"), "id_ID.UTF-8"},
	{N_("Italian"), "it_IT.UTF-8"},
	{N_("Japanese"), "ja_JP.UTF-8"},
	{N_("Lithuanian"), "lt_LT.UTF-8"},
	{N_("Norwegian"), "no_NO.UTF-8"},
	{N_("Polish"), "pl_PL.UTF-8"},
	{N_("Portuguese"), "pt_PT.UTF-8"},
	{N_("Romanian"), "ro_RO.UTF-8"},
	{N_("Russian"), "ru_RU.UTF-8"},
	{N_("Serbian"), "sr_RS.UTF-8"},
	{N_("Slovak"), "sk_SK.UTF-8"},
	{N_("Spanish"), "es_ES.UTF-8"},
	{N_("Swedish"), "sv_SE.UTF-8"},
	{N_("Turkish"), "tr_TR.UTF-8"},
	{N_("Ukrainian"), "uk_UA.UTF-8"},

	{NULL, NULL},
};

/* XXX: In fact this is _NOT_ a real ISO639 code but RFC3066 code (as we're
 * supposed to use that one when sending language tags through HTTP/1.1) and
 * that one consists basically from ISO639[-ISO3166].  This is important for
 * ie. pt vs pt-BR. */
/* TODO: We should reflect this in name of this function and of the tag. On the
 * other side, it's ISO639 for gettext as well etc. So what?  --pasky */

int
iso639_to_language(char *iso639)
{
	char *l = stracpy(iso639);
	char *p;
	int i, ll;

	if (!l)
		return 1;

	/* The environment variable transformation. */

	p = strchr((const char *)l, '.');
	if (p)
		*p = '\0';

	p = strchr((const char *)l, '_');
	if (p)
		*p = '-';
	else
		p = strchr((const char *)l, '-');

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

char *
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
name_to_language(const char *name)
{
	int i;

	for (i = 0; languages[i].name; i++) {
		if (c_strcasecmp(languages[i].name, name))
			continue;
		return i;
	}
	return 1;
}

char *
language_to_name(int language)
{
	return languages[language].name;
}

int
get_system_language_index(void)
{
	char *l;

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

char *EL_LANGUAGE;

void
set_language(int language)
{
	char *p;
	struct string lang;
	int charset;

	if (!system_language)
		system_language = get_system_language_index();

	if (language == current_language) {
		/* Nothing to do. */
		return;
	}

	current_language = language;

	if (!language)
		language = system_language;

	if (!EL_LANGUAGE) {
		/* We never free() this, purely intentionally. */
		EL_LANGUAGE = malloc(256);
	}
	if (EL_LANGUAGE) {
		strcpy(EL_LANGUAGE, language_to_iso639(language));
		p = strchr((const char *)EL_LANGUAGE, '-');
		if (p) {
			*p = '_';
		}
	}

	if (!init_string(&lang)) {
		return;
	}
	add_to_string(&lang, "LC_ALL=");
	add_to_string(&lang, EL_LANGUAGE);
	putenv(lang.source);
	done_string(&lang);

	setlocale(LC_ALL, EL_LANGUAGE);
	bindtextdomain(PACKAGE, LOCALEDIR);
	charset = current_charset;
	current_charset = -1;
	intl_set_charset_by_index(charset);
	textdomain(PACKAGE);
}
