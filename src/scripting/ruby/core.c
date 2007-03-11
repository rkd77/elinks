/* Ruby interface (scripting engine) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ruby.h>

#undef _

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/home.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "osdep/osdep.h"
#include "scripting/scripting.h"
#include "scripting/ruby/core.h"
#include "scripting/ruby/ruby.h"
#include "util/error.h"
#include "util/file.h"
#include "util/string.h"

#define RUBY_HOOKS_FILENAME	"hooks.rb"


/* I've decided to use `erb' to refer to this ELinks/ruby thingy. */

VALUE erb_module;


/* Error reporting. */

void
alert_ruby_error(struct session *ses, unsigned char *msg)
{
	report_scripting_error(&ruby_scripting_module, ses, msg);
}

/* Another Vim treat. */
void
erb_report_error(struct session *ses, int error)
{
	VALUE eclass;
	VALUE einfo;
	unsigned char buff[MAX_STR_LEN];
	unsigned char *msg;

	/* XXX: Ew. These are from the Ruby internals. */
#define TAG_RETURN	0x1
#define TAG_BREAK	0x2
#define TAG_NEXT	0x3
#define TAG_RETRY	0x4
#define TAG_REDO	0x5
#define TAG_RAISE	0x6
#define TAG_THROW	0x7
#define TAG_FATAL	0x8
#define TAG_MASK	0xf

	switch (error) {
	case TAG_RETURN:
		msg = "unexpected return";
		break;
	case TAG_NEXT:
		msg = "unexpected next";
		break;
	case TAG_BREAK:
		msg = "unexpected break";
		break;
	case TAG_REDO:
		msg = "unexpected redo";
		break;
	case TAG_RETRY:
		msg = "retry outside of rescue clause";
		break;
	case TAG_RAISE:
	case TAG_FATAL:
		eclass = CLASS_OF(ruby_errinfo);
		einfo = rb_obj_as_string(ruby_errinfo);

		if (eclass == rb_eRuntimeError && RSTRING(einfo)->len == 0) {
			msg = "unhandled exception";

		} else {
			VALUE epath;
			unsigned char *p;

			epath = rb_class_path(eclass);
			snprintf(buff, MAX_STR_LEN, "%s: %s",
				RSTRING(epath)->ptr, RSTRING(einfo)->ptr);

			p = strchr(buff, '\n');
			if (p) *p = '\0';
			msg = buff;
		}
		break;
	default:
		snprintf(buff, MAX_STR_LEN, "unknown longjmp status %d", error);
		msg = buff;
		break;
	}

	alert_ruby_error(ses, msg);
}


/* The ELinks module: */

/* Inspired by Vim this is used to hook into the stdout write method so written
 * data is displayed in a nice message box. */
static VALUE
erb_module_message(VALUE self, VALUE str)
{
	unsigned char *message, *line_end;

	str = rb_obj_as_string(str);
	message = memacpy(RSTRING(str)->ptr, RSTRING(str)->len);
	if (!message) return Qnil;

	line_end = strchr(message, '\n');
	if (line_end) *line_end = '\0';

	if (list_empty(terminals)) {
		usrerror("[Ruby] %s", message);
		mem_free(message);
		return Qnil;
	}

	info_box(terminals.next, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
		 N_("Ruby Message"), ALIGN_LEFT, message);

	return Qnil;
}

/* The global Kernel::p method will for each object, directly write
 * object.inspect() followed by the current output record separator to the
 * program's standard output and will bypass the Ruby I/O libraries.
 *
 * Inspired by Vim we hook into the method and pop up a nice message box so it
 * can be used to easily debug scripts without dirtying the screen. */
static VALUE
erb_stdout_p(int argc, VALUE *argv, VALUE self)
{
	int i;
	struct string string;

	if (!init_string(&string))
		return Qnil;

	for (i = 0; i < argc; i++) {
		VALUE substr;
		unsigned char *ptr;
		int len;

		if (i > 0)
			add_to_string(&string, ", ");

		substr = rb_inspect(argv[i]);

		/* The Ruby p() function writes variable number of objects using
		 * the inspect() method, which adds quotes to the strings, so
		 * gently ignore them. */

		ptr = RSTRING(substr)->ptr;
		len = RSTRING(substr)->len;

		if (*ptr == '"')
			ptr++, len--;

		if (ptr[len - 1] == '"')
			len--;

		add_bytes_to_string(&string, ptr, len);
	}

	if (list_empty(terminals)) {
		usrerror("[Ruby] %s", string.source);
		done_string(&string);
		return Qnil;
	}

	info_box(terminals.next, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
		N_("Ruby Message"), ALIGN_LEFT, string.source);

	return Qnil;
}

/* ELinks::method_missing() is a catch all method that will be called when a
 * hook is not defined. It might not be so elegant but it removes NoMethodErrors
 * from popping up. */
/* FIXME: It might be useful for user to actually display them to debug scripts,
 * so maybe it should be optional. --jonas */
static VALUE
erb_module_method_missing(VALUE self, VALUE arg)
{
	return Qnil;
}

static void
init_erb_module(void)
{
	unsigned char *home;

	erb_module = rb_define_module("ELinks");
	rb_define_const(erb_module, "VERSION", rb_str_new2(VERSION_STRING));

	home = elinks_home ? elinks_home : (unsigned char *) CONFDIR;
	rb_define_const(erb_module, "HOME", rb_str_new2(home));

	rb_define_module_function(erb_module, "message", erb_module_message, 1);
	rb_define_module_function(erb_module, "method_missing", erb_module_method_missing, -1);
	rb_define_module_function(erb_module, "p", erb_stdout_p, -1);
}


void
init_ruby(struct module *module)
{
	unsigned char *path;

	/* Set up and initialize the interpreter. This function should be called
	 * before any other Ruby-related functions. */
	ruby_init();
	ruby_script("ELinks-ruby");
	ruby_init_loadpath();

	/* ``Trap'' debug prints from scripts. */
	rb_define_singleton_method(rb_stdout, "write", erb_module_message, 1);
	rb_define_global_function("p", erb_stdout_p, -1);

	/* Set up the ELinks module interface. */
	init_erb_module();

	if (elinks_home) {
		path = straconcat(elinks_home, RUBY_HOOKS_FILENAME,
				  (unsigned char *) NULL);

	} else {
		path = stracpy(CONFDIR STRING_DIR_SEP RUBY_HOOKS_FILENAME);
	}

	if (!path) return;

	if (file_can_read(path)) {
		int error;

		/* Load ~/.elinks/hooks.rb into the interpreter. */
		//rb_load_file(path);
		rb_load_protect(rb_str_new2(path), 0, &error);
		if (error)
			erb_report_error(NULL, error);
	}

	mem_free(path);
}
