/* ECMAScript browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "ecmascript/spidermonkey-shared.h"
#include <js/Printf.h>
#include "intl/charsets.h"
#include "main/module.h"
#include "osdep/osdep.h"
#include "scripting/scripting.h"
#include "scripting/smjs/core.h"
#include "scripting/smjs/elinks_object.h"
#include "scripting/smjs/global_object.h"
#include "scripting/smjs/smjs.h"
#include "util/file.h"
#include "util/string.h"

#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/Warnings.h>

#define SMJS_HOOKS_FILENAME "hooks.js"

JSContext *smjs_ctx;
JSObject *smjs_elinks_object;
struct session *smjs_ses;

void
alert_smjs_error(const char *msg)
{
	report_scripting_error(&smjs_scripting_module,
	                       smjs_ses, msg);
}

static void
error_reporter(JSContext *ctx, JSErrorReport *report)
{
	char *ptr;
	size_t size;

	FILE *f = open_memstream(&ptr, &size);

	if (f) {
		struct string msg;
		JS::PrintError(f, report, true/*reportWarnings*/);
		fclose(f);

		if (!init_string(&msg)) {
			free(ptr);
			return;
		}
		add_to_string(&msg, "A client script raised the following:\n");
		add_bytes_to_string(&msg, ptr, size);
		free(ptr);
		alert_smjs_error(msg.source);
		done_string(&msg);
	}

	JS_ClearPendingException(ctx);
}

static int
smjs_do_file(char *path)
{
	int ret = 1;
	struct string script;

	if (!init_string(&script)) return 0;

	JS::CompileOptions opts(smjs_ctx);
	opts.setNoScriptRval(true);
	JS::RootedValue rval(smjs_ctx);

	JSAutoRealm ar(smjs_ctx, smjs_elinks_object);

	if (add_file_to_string(&script, path)) {
		JS::SourceText<mozilla::Utf8Unit> srcBuf;

		if (!srcBuf.init(smjs_ctx, script.source, script.length, JS::SourceOwnership::Borrowed)) {
			alert_smjs_error("error loading script file");
			ret = 0;
		} else {
			if (!JS::Evaluate(smjs_ctx, opts,
				srcBuf, &rval)) {
				alert_smjs_error("error loading script file");
				ret = 0;
			}
		}
	} else {
		alert_smjs_error("error loading script file");
		ret = 0;
	}

	done_string(&script);

	return ret;
}

static bool
smjs_do_file_wrapper(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::CallArgs args = CallArgsFromVp(argc, rval);

	char *path = jsval_to_string(smjs_ctx, args[0]);

	if (smjs_do_file(path))
		return true;

	return false;
}

static void
smjs_load_hooks(void)
{
	char *path;
	char *xdg_config_home = get_xdg_config_home();

	assert(smjs_ctx);

	if (xdg_config_home) {
		path = straconcat(xdg_config_home, SMJS_HOOKS_FILENAME,
				  (char *) NULL);
	} else {
		path = stracpy(CONFDIR STRING_DIR_SEP SMJS_HOOKS_FILENAME);
	}

	if (file_exists(path))
		smjs_do_file(path);
	mem_free(path);
}

void
init_smjs(struct module *module)
{
	if (!spidermonkey_runtime_addref()) return;

	smjs_ctx = main_ctx; //JS_NewContext(8L * 1024 * 1024);
	if (!smjs_ctx) {
		spidermonkey_runtime_release();
		return;
	}

	JS::SetWarningReporter(smjs_ctx, error_reporter);

	smjs_init_global_object();

	smjs_init_elinks_object();

	JS::RootedObject r_smjs_global_object(smjs_ctx, smjs_global_object->get());
	JSAutoRealm ar(smjs_ctx, r_smjs_global_object);

	JS_DefineFunction(smjs_ctx, r_smjs_global_object, "do_file",
	                  &smjs_do_file_wrapper, 1, 0);

	smjs_load_hooks();
}

void
cleanup_smjs(struct module *module)
{
	if (!smjs_ctx) return;

	/* JS_DestroyContext also collects garbage in the JSRuntime.
	 * Because the JSObjects created in smjs_ctx have not been
	 * made visible to any other JSContext, and the garbage
	 * collector of SpiderMonkey is precise, SpiderMonkey
	 * finalizes all of those objects, so cache_entry_finalize
	 * gets called and resets each cache_entry.jsobject = NULL.
	 * If the garbage collector were conservative, ELinks would
	 * have to call smjs_detach_cache_entry_object on each cache
	 * entry before it releases the runtime here.  */
	//JS_DestroyContext(smjs_ctx);
	spidermonkey_runtime_release();
}

/** Convert a UTF-8 string to a JSString.
 *
 * @param ctx
 *   Allocate the string in this JSContext.
 * @param[in] str
 *   The input string that should be converted.
 * @param[in] length
 *   Length of @a str in bytes, or -1 if it is null-terminated.
 *
 * @return the new string.  On error, report the error to SpiderMonkey
 * and return NULL.  */
JSString *
utf8_to_jsstring(JSContext *ctx, const char *str, int length)
{
	size_t in_bytes;

	if (length == -1)
		in_bytes = strlen(str);
	else
		in_bytes = length;

	JS::ConstUTF8CharsZ utf8chars(str, in_bytes);

	return JS_NewStringCopyUTF8Z(ctx, utf8chars);
}

/** Convert a char16_t array to UTF-8 and append it to struct string.
 * Replace misused surrogate codepoints with UCS_REPLACEMENT_CHARACTER.
 *
 * @param[in,out] utf8
 *   The function appends characters to this UTF-8 string.
 *
 * @param[in] utf16
 *   Pointer to the first element in an array of char16_ts.
 *
 * @param[in] len
 *   Number of char16_ts in the @a utf16 array.
 *
 * @return @a utf8 if successful, or NULL if not.  */
static struct string *
add_jschars_to_utf8_string(struct string *utf8,
			   const char16_t *utf16, size_t len)
{
	size_t pos;

	for (pos = 0; pos < len; ) {
		unicode_val_T unicode = utf16[pos++];

		if (is_utf16_surrogate(unicode)) {
			if (is_utf16_high_surrogate(unicode)
			    && pos < len
			    && is_utf16_low_surrogate(utf16[pos])) {
				unicode = join_utf16_surrogates(unicode,
								utf16[pos++]);
			} else {
				unicode = UCS_REPLACEMENT_CHARACTER;
			}
		}

		if (unicode) {
			if (!add_to_string(utf8, encode_utf8(unicode)))
				return NULL;
		}
	}

	return utf8;
}

/** Convert a JSString to a UTF-8 string.
 *
 * @param ctx
 *   For reporting errors.
 * @param[in] jsstr
 *   The input string that should be converted.  Must not be NULL.
 * @param[out] length
 *   Optional.  The number of bytes in the returned string,
 *   not counting the terminating null.
 *
 * @return the new string, which the caller must eventually free
 * with mem_free().  On error, report the error to SpiderMonkey
 * and return NULL; *@a length is then undefined.  */
char *
jsstring_to_utf8(JSContext *ctx, JSString *jsstr, int *length)
{
	size_t utf16_len;
	const char16_t *utf16;
	struct string utf8;

	utf16_len = JS_GetStringLength(jsstr);
	utf16 = JS_GetTwoByteExternalStringChars(jsstr); /* stays owned by jsstr */
	if (utf16 == NULL) {
		/* JS_GetStringChars doesn't have a JSContext *
		 * parameter so it can't report the error
		 * (and can't collect garbage either).  */
		JS_ReportOutOfMemory(ctx);
		return NULL;
	}

	if (!init_string(&utf8)) {
		JS_ReportOutOfMemory(ctx);
		return NULL;
	}

	if (!add_jschars_to_utf8_string(&utf8, utf16, utf16_len)) {
		done_string(&utf8);
		JS_ReportOutOfMemory(ctx);
		return NULL;
	}

	if (length)
		*length = utf8.length;
	return utf8.source;
}
