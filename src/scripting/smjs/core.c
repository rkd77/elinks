/* ECMAScript browser scripting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "ecmascript/spidermonkey-shared.h"
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


#define SMJS_HOOKS_FILENAME "hooks.js"

JSContext *smjs_ctx;
JSObject *smjs_elinks_object;
struct session *smjs_ses;


void
alert_smjs_error(unsigned char *msg)
{
	report_scripting_error(&smjs_scripting_module,
	                       smjs_ses, msg);
}

static void
error_reporter(JSContext *ctx, const char *message, JSErrorReport *report)
{
	unsigned char *strict, *exception, *warning, *error;
	struct string msg;

	if (!init_string(&msg)) goto reported;

	strict	  = JSREPORT_IS_STRICT(report->flags) ? " strict" : "";
	exception = JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "";
	warning   = JSREPORT_IS_WARNING(report->flags) ? " warning" : "";
	error	  = !report->flags ? " error" : "";

	add_format_to_string(&msg, "A client script raised the following%s%s%s%s",
			strict, exception, warning, error);

	add_to_string(&msg, ":\n\n");
	add_to_string(&msg, message);

	if (report->linebuf && report->tokenptr) {
		int pos = report->tokenptr - report->linebuf;

		add_format_to_string(&msg, "\n\n%s\n.%*s^%*s.",
			       report->linebuf,
			       pos - 2, " ",
			       strlen(report->linebuf) - pos - 1, " ");
	}

	alert_smjs_error(msg.source);
	done_string(&msg);

reported:
	JS_ClearPendingException(ctx);
}

static int
smjs_do_file(unsigned char *path)
{
	int ret = 1;
	jsval rval;
	struct string script;

	if (!init_string(&script)) return 0;

	if (!add_file_to_string(&script, path)
	     || JS_FALSE == JS_EvaluateScript(smjs_ctx,
				JS_GetGlobalObject(smjs_ctx),
				script.source, script.length, path, 1, &rval)) {
		alert_smjs_error("error loading script file");
		ret = 0;
	}

	done_string(&script);

	return ret;
}

static JSBool
smjs_do_file_wrapper(JSContext *ctx, JSObject *obj, uintN argc,
                     jsval *argv, jsval *rval)
{
	JSString *jsstr = JS_ValueToString(smjs_ctx, *argv);
	unsigned char *path = JS_GetStringBytes(jsstr);

	if (smjs_do_file(path))
		return JS_TRUE;

	return JS_FALSE;
}

static void
smjs_load_hooks(void)
{
	unsigned char *path;

	assert(smjs_ctx);

	if (elinks_home) {
		path = straconcat(elinks_home, SMJS_HOOKS_FILENAME,
				  (unsigned char *) NULL);
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

	smjs_ctx = JS_NewContext(spidermonkey_runtime, 8192);
	if (!smjs_ctx) {
		spidermonkey_runtime_release();
		return;
	}

	JS_SetErrorReporter(smjs_ctx, error_reporter);

	smjs_init_global_object();

	smjs_init_elinks_object();

	JS_DefineFunction(smjs_ctx, smjs_global_object, "do_file",
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
	JS_DestroyContext(smjs_ctx);
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
utf8_to_jsstring(JSContext *ctx, const unsigned char *str, int length)
{
	size_t in_bytes;
	const unsigned char *in_end;
	size_t utf16_alloc;
	jschar *utf16;
	size_t utf16_used;
	JSString *jsstr;

	if (length == -1)
		in_bytes = strlen(str);
	else
		in_bytes = length;

	/* Each byte of input can become at most one UTF-16 unit.
	 * Check whether the multiplication could overflow.  */
	assert(!needs_utf16_surrogates(UCS_REPLACEMENT_CHARACTER));
	if (in_bytes > ((size_t) -1) / sizeof(jschar)) {
#ifdef HAVE_JS_REPORTALLOCATIONOVERFLOW
		JS_ReportAllocationOverflow(ctx);
#else
		JS_ReportOutOfMemory(ctx);
#endif
		return NULL;
	}
	utf16_alloc = in_bytes;
	/* Use malloc because SpiderMonkey will handle the memory after
	 * this routine finishes.  */
	utf16 = malloc(utf16_alloc * sizeof(jschar));
	if (utf16 == NULL) {
		JS_ReportOutOfMemory(ctx);
		return NULL;
	}

	in_end = str + in_bytes;

	utf16_used = 0;
	for (;;) {
		unicode_val_T unicode;

		unicode = utf8_to_unicode((unsigned char **) &str, in_end);
		if (unicode == UCS_NO_CHAR)
			break;

		if (needs_utf16_surrogates(unicode)) {
			assert(utf16_alloc - utf16_used >= 2);
			if_assert_failed { free(utf16); return NULL; }
			utf16[utf16_used++] = get_utf16_high_surrogate(unicode);
			utf16[utf16_used++] = get_utf16_low_surrogate(unicode);
		} else {
			assert(utf16_alloc - utf16_used >= 1);
			if_assert_failed { free(utf16); return NULL; }
			utf16[utf16_used++] = unicode;
		}
	}

	jsstr = JS_NewUCString(ctx, utf16, utf16_used);
	/* Do not free if JS_NewUCString was successful because it takes over
	 * handling of the memory. */
	if (jsstr == NULL) free(utf16);

	return jsstr;
}

/** Convert a jschar array to UTF-8 and append it to struct string.
 * Replace misused surrogate codepoints with UCS_REPLACEMENT_CHARACTER.
 *
 * @param[in,out] utf8
 *   The function appends characters to this UTF-8 string.
 *
 * @param[in] utf16
 *   Pointer to the first element in an array of jschars.
 *
 * @param[in] len
 *   Number of jschars in the @a utf16 array.
 *
 * @return @a utf8 if successful, or NULL if not.  */
static struct string *
add_jschars_to_utf8_string(struct string *utf8,
			   const jschar *utf16, size_t len)
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

		if (unicode == 0) {
			if (!add_char_to_string(utf8, '\0'))
				return NULL;
		} else {
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
unsigned char *
jsstring_to_utf8(JSContext *ctx, JSString *jsstr, int *length)
{
	size_t utf16_len;
	const jschar *utf16;
	struct string utf8;

	utf16_len = JS_GetStringLength(jsstr);
	utf16 = JS_GetStringChars(jsstr); /* stays owned by jsstr */
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
