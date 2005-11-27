/* The ELinks SEE interface: */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <see/see.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/conf.h"
#include "config/home.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "scripting/scripting.h"
#include "scripting/see/core.h"
#include "scripting/see/see.h"
#include "session/session.h"
#include "util/error.h"
#include "util/file.h"
#include "util/string.h"


static void
navigator_preference(struct SEE_interpreter *see, struct SEE_object *self,
		     struct SEE_object *thisobj, int argc, struct SEE_value **argv,
		     struct SEE_value *res)
{
        struct SEE_value v;
	struct string opt_name;
	struct option *opt;

	SEE_SET_UNDEFINED(res);

        if (argc != 1 && argc != 2) return;

	SEE_ToString(see, argv[0], &v);
	if (!convert_see_string(&opt_name, v.u.string))
		return;

	opt = get_opt_rec(config_options, opt_name.source);
	done_string(&opt_name);
	/* FIXME: Alert? */
	if (!opt) return;

	/* Set option */
	switch (opt->type) {
	case OPT_BOOL:
	{
		long value = opt->value.number;

		if (argc == 1) {
			SEE_SET_BOOLEAN(res, value);
			return;
		}

		SEE_ToBoolean(see, argv[1], &v);
		value = !!v.u.boolean;
		opt->value.number = value;
		break;
	}
	case OPT_INT:
	case OPT_LONG:
	{
		long value;

		if (argc == 1) {
			SEE_SET_NUMBER(res, opt->value.number);
			return;
		}

		SEE_ToInteger(see, argv[1], &v);
		value = SEE_ToInt32(see, &v);
		if (opt->min <= value && value <= opt->max)
			option_types[opt->type].set(opt, (unsigned char *) (&value));
		break;
	}
	case OPT_STRING:
	case OPT_CODEPAGE:
	case OPT_LANGUAGE:
	case OPT_COLOR:
	{
		struct string opt_value;

		if (argc == 1) {
			SEE_SET_STRING(res, SEE_string_sprintf(see, opt->value.string));
			return;
		}

		SEE_ToString(see, argv[1], &v);
		if (!convert_see_string(&opt_value, v.u.string))
			return;

		option_types[opt->type].set(opt, opt_value.source);
		done_string(&opt_value);
		break;
	}
	default:
		return;
	}

	if (argc == 2) {
		opt->flags |= OPT_TOUCHED;
		call_change_hooks(see_ses, opt, opt);
	}
}

static void
navigator_save_preferences(struct SEE_interpreter *see, struct SEE_object *self,
			   struct SEE_object *thisobj, int argc, struct SEE_value **argv,
			   struct SEE_value *res)
{
	if (see_ses)
		write_config(see_ses->tab->term);
}

static void
navigator_alert(struct SEE_interpreter *see, struct SEE_object *self,
		 struct SEE_object *thisobj, int argc, struct SEE_value **argv,
		 struct SEE_value *res)
{
	struct SEE_value v;
	struct string string;
	struct terminal *term;

	SEE_SET_UNDEFINED(res);

	if (!argc) return;

	SEE_ToString(see, argv[0], &v);
	if (!convert_see_string(&string, v.u.string))
		return;

	if (!see_ses && list_empty(terminals)) {
		usrerror("[SEE] %s", string.source);
		done_string(&string);
		return;
	}

	term = see_ses ? see_ses->tab->term : terminals.next;

	info_box(term, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
		 N_("SEE Message"), ALIGN_LEFT, string.source);
}

#if DATADRIVEN
_IDEA
struct object_info browser_object[] = {
	"ELinks", SEE_ATTR_READONLY,
	{ /* Properties: */
		{ "version",	SEE_STRING,	VERSION,	SEE_ATTR_READONLY },
		{ "home",	SEE_STRING,	NULL,		SEE_ATTR_READONLY },
	},
	{ /* Methods: (as name, handler, args) */
		{ "write",	elinks_see_write, SEE_ATTR_READONLY },
		{ NULL }
	},
};

struct object_info *see_
#endif

void
init_see_interface(struct SEE_interpreter *see)
{
	unsigned char *home;
	struct SEE_object *obj, *navigator;
	struct SEE_value value;
	struct SEE_string *name;

	/* Create the navigator browser object. Add it to the global space */
	navigator = SEE_Object_new(see);
	SEE_SET_OBJECT(&value, navigator);
	name = SEE_string_sprintf(see, "navigator");
	SEE_OBJECT_PUT(see, see->Global, name, &value, SEE_ATTR_READONLY);

	/* Create a string and attach as 'ELinks.version' */
	SEE_SET_STRING(&value, SEE_string_sprintf(see, VERSION));
	name = SEE_string_sprintf(see, "appVersion");
	SEE_OBJECT_PUT(see, navigator, name, &value, SEE_ATTR_READONLY);

	/* Create a string and attach as 'ELinks.home' */
	home = elinks_home ? elinks_home : (unsigned char *) CONFDIR;
	SEE_SET_STRING(&value, SEE_string_sprintf(see, home));
	name = SEE_string_sprintf(see, "appHome");
	SEE_OBJECT_PUT(see, navigator, name, &value, SEE_ATTR_READONLY);

	/* Create an 'alert' method and attach to the browser object. */
	/* FIXME: The browser object and the Global object should be identical. */
	name = SEE_string_sprintf(see, "alert");
	obj = SEE_cfunction_make(see, navigator_alert, name, 1);
	SEE_SET_OBJECT(&value, obj);
	SEE_OBJECT_PUT(see, navigator, name, &value, 0);
	SEE_OBJECT_PUT(see, see->Global, name, &value, 0);

	name = SEE_string_sprintf(see, "preference");
	obj = SEE_cfunction_make(see, navigator_preference, name, 1);
	SEE_SET_OBJECT(&value, obj);
	SEE_OBJECT_PUT(see, navigator, name, &value, 0);

	name = SEE_string_sprintf(see, "savePreferences");
	obj = SEE_cfunction_make(see, navigator_save_preferences, name, 1);
	SEE_SET_OBJECT(&value, obj);
	SEE_OBJECT_PUT(see, navigator, name, &value, 0);
}

