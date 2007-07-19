#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "protocol/test/harness.h"
#include "protocol/uri.h"
#include "util/string.h"

static int failures = 0;
static int successes = 0;

void
test_failed(void)
{
	++failures;
}

void
test_succeeded(void)
{
	++successes;
}

static void
test_1_normalize_uri(const unsigned char *orig, const unsigned char *good)
{
	struct string s;
	unsigned char *norm;

	if (!init_string(&s)) {
		fputs("FAIL: init_string\n", stderr);
		test_failed();
		goto out;
	}
	if (!add_to_string(&s, orig)) {
		fputs("FAIL: add_to_string\n", stderr);
		test_failed();
		goto out;
	}

	norm = normalize_uri(NULL, s.source);
	if (norm == NULL) {
		fprintf(stderr, "FAIL: normalize_uri NULL %s\n", orig);
		test_failed();
		goto out;
	}
	if (strcmp(norm, good) != 0) {
		fprintf(stderr, "FAIL: normalize_uri mismatch:\n"
			"\toriginal: %s\n"
			"\tresult:   %s\n"
			"\texpected: %s\n",
			orig, norm, good);
		test_failed();
		goto out;
	}

	test_succeeded();

out:
	done_string(&s);
}

static void
test_normalize_uri(void)
{
	static const struct {
		unsigned char *orig;
		unsigned char *norm;
	} tests[] = {
		{ "http://example.org/foo/bar/baz?a=1&b=2#frag",
		  "http://example.org/foo/bar/baz?a=1&b=2#frag" },
		{ "http://example.org/foo/bar/../?a=1&b=2#frag",
		  "http://example.org/foo/?a=1&b=2#frag" },
		{ "http://example.org/foo/bar/../../baz?a=1&b=2#frag",
		  "http://example.org/baz?a=1&b=2#frag" },
		{ "http://example.org/foo/bar/..",
		  "http://example.org/foo/" },
		{ "http://example.org/foo/bar;a=1/..",
		  "http://example.org/foo/" },
		{ "http://example.org/foo/bar..",
		  "http://example.org/foo/bar.." },

		/* Bug 744 - ELinks changes "//" to "/" in path
		 * component of URI */
		{ "http://example.org/foo/bar/baz",
		  "http://example.org/foo/bar/baz" },
		{ "http://example.org/foo/bar/",
		  "http://example.org/foo/bar/" },
		{ "http://example.org/foo//baz",
		  "http://example.org/foo//baz" },
		{ "http://example.org/foo//",
		  "http://example.org/foo//" },
		{ "http://example.org//bar/baz",
		  "http://example.org//bar/baz" },
		{ "http://example.org//bar/",
		  "http://example.org//bar/" },
		{ "http://example.org///baz",
		  "http://example.org///baz" },
		{ "http://example.org///",
		  "http://example.org///" },
		{ "http://example.org/foo/bar/baz/..",
		  "http://example.org/foo/bar/" },
		{ "http://example.org/foo/bar//..",
		  "http://example.org/foo/bar/" },
		{ "http://example.org/foo//baz/..",
		  "http://example.org/foo//" },
		{ "http://example.org/foo///..",
		  "http://example.org/foo//" },
		{ "http://example.org//bar/baz/..",
		  "http://example.org//bar/" },
		{ "http://example.org//bar//..",
		  "http://example.org//bar/" },
		{ "http://example.org///baz/..",
		  "http://example.org///" },
		{ "http://example.org////..",
		  "http://example.org///" },
		{ "http://example.org/foo/..//bar/baz",
		  "http://example.org//bar/baz" },
		{ "http://example.org//.//foo",
		  "http://example.org///foo" },
		{ "http://example.org//./../foo",
		  "http://example.org/foo" },
		{ "http://example.org/gag///./../..",
		  "http://example.org/gag/" },
	};
	size_t i;

	for (i = 0; i < sizeof_array(tests); ++i)
		test_1_normalize_uri(tests[i].orig, tests[i].norm);
}

int
main(int argc, char **argv)
{
	test_normalize_uri();
	printf("Total %d failures, %d successes.\n", failures, successes);
	return failures ? EXIT_FAILURE : 0;
}
