/* Test parsing of <meta http-equiv="refresh" content="..."> */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parse-meta-refresh.h"
#include "util/memory.h"

struct meta_refresh_test_case
{
	const unsigned char *content;
	int error;
	unsigned long delay;
	const unsigned char *url;
};

static const struct meta_refresh_test_case meta_refresh_test_cases[] = {
	/* delay only */
	{ "42",
	  0, 42, NULL },
	{ "0",
	  0, 0, NULL },
	{ "   5   ",
	  0, 5, NULL },
	{ "9999999999999999999999999",
	  0, ULONG_MAX, NULL },
	{ "69 ; ",
	  0, 69, NULL },
	{ "105;",
	  0, 105, NULL },
	{ "",
	  -1, 0, NULL },

	/* blank URL; these match Iceweasel/3.5.16 */
	{ "5; URL=''",
	  0, 5, NULL },
	{ "; URL=''",
	  -1, 0, NULL },

	/* simple; these match Iceweasel/3.5.16 */
	{ "42; URL=file:///dir/file.html",
	  0, 42, "file:///dir/file.html" },
	{ "42; URL='file:///dir/file.html'",
	  0, 42, "file:///dir/file.html" },
	{ "42; URL=\"file:///dir/file.html\"",
	  0, 42, "file:///dir/file.html" },

	/* without URL=; these match Iceweasel/3.5.16 */
	{ "9; file:///dir/file.html",
	  0, 9, "file:///dir/file.html" },
	{ "9; 'file:///dir/file.html'",
	  0, 9, "file:///dir/file.html" },
	{ "9; \"file:///dir/file.html\"",
	  0, 9, "file:///dir/file.html" },

	/* lower case; these match Iceweasel/3.5.16 */
	{ "3; Url=\"file:///dir/file.html\"",
	  0, 3, "file:///dir/file.html" },
	{ "3; url=\"file:///dir/file.html\"",
	  0, 3, "file:///dir/file.html" },

	/* unusual delimiters; these match Iceweasel/3.5.16 */
	{ "0 URL=\"file:///dir/file.html\"",
	  0, 0, "file:///dir/file.html" },
	{ "0  ;  URL  =  \"file:///dir/file.html\"",
	  0, 0, "file:///dir/file.html" },
	{ "1, URL=\"file:///dir/file.html\"",
	  0, 1, "file:///dir/file.html" },
	{ "+0 URL='file:///dir/file.html'",
	  0, 0, "file:///dir/file.html" },
	{ "+0 URL=foo; URL='file:///dir/file.html'; URL='bar'",
	  0, 0, "foo; URL='file:///dir/file.html'; URL='bar'" },
	{ "+ URL=foo; URL='file:///dir/file.html'; URL='bar'",
	  0, 0, "file:///dir/file.html" },
	{ ". URL=foo; URL='file:///dir/file.html'; URL='bar'",
	  0, 0, "file:///dir/file.html" },
	{ ".0 URL=foo; URL='file:///dir/file.html'; URL='bar'",
	  0, 0, "file:///dir/file.html" },
	{ "0. URL=foo; URL='file:///dir/file.html'; URL='bar'",
	  0, 0, "foo; URL='file:///dir/file.html'; URL='bar'" },
	{ "4URL=foo; URL='file:///dir/file.html'; URL='bar'",
	  -1, 0, NULL },
	{ "garbage URL='file:///dir/file.html'",
	  -1, 0, NULL },

	/* semicolons in the URL; these match Iceweasel/3.5.16 */
	{ "3; URL=file:///dir/file.cgi?a=1;b=2;c=3",
	  0, 3, "file:///dir/file.cgi?a=1;b=2;c=3" },
	{ "3; URL=\"file:///dir/file.cgi?a=1;b=2;c=3\"",
	  0, 3, "file:///dir/file.cgi?a=1;b=2;c=3" },

	/* spaces in the URL; these match Iceweasel/3.5.16 */
	{ "3; URL=\"file:///dir/file.cgi?phrase=Hello, world!\"",
	  0, 3, "file:///dir/file.cgi?phrase=Hello, world!" },
	{ "3; URL=\"file:///dir/file.cgi?phrase=Hello, world!  \"",
	  0, 3, "file:///dir/file.cgi?phrase=Hello, world!" },
	{ "3; URL=\"file:///dir/file.cgi?phrase=Hello, world! %20 \"",
	  0, 3, "file:///dir/file.cgi?phrase=Hello, world! %20" },
	{ "3; URL=file:///dir/file.cgi?phrase=Hello, world!",
	  0, 3, "file:///dir/file.cgi?phrase=Hello, world!" },
	{ "3; URL=file:///dir/file.cgi?phrase=Hello, world! ",
	  0, 3, "file:///dir/file.cgi?phrase=Hello, world!" },

	/* "URL" in the URL; these match Iceweasel/3.5.16 */
	{ "0; URL=file:///dir/xlat.cgi?url=http://example.org/&lang=cu",
	  0, 0, "file:///dir/xlat.cgi?url=http://example.org/&lang=cu" },
	{ "0; file:///dir/xlat.cgi?url=http://example.org/&lang=cu",
	  0, 0, "file:///dir/xlat.cgi?url=http://example.org/&lang=cu" },

	/* unusual delays; these sort-of match Iceweasel/3.5.16,
	 * except it was not tested whether Iceweasel truncates the
	 * delay to an integer, and it was not tested how long the
	 * delays get with negative numbers.  */
	{ "; URL=\"file:///dir/file.html\"",
	  0, 0, "file:///dir/file.html" },
	{ "2.99999; file:///dir/file.html",
	  0, 2, "file:///dir/file.html" },
	{ "2.99999; 'file:///dir/file.html'",
	  0, 2, "file:///dir/file.html" },
	{ "040; URL='file:///dir/file.html'",
	  0, 40, "file:///dir/file.html" },
	{ "+4; URL='file:///dir/file.html'",
	  0, 4, "file:///dir/file.html" },
	{ "  2; URL='file:///dir/file.html'",
	  0, 2, "file:///dir/file.html" },
	{ "+0; URL='file:///dir/file.html'",
	  0, 0, "file:///dir/file.html" },
	{ "-0; URL='file:///dir/file.html'",
	  0, 0, "file:///dir/file.html" },
	{ "-0.1; URL='file:///dir/file.html'",
	  0, 0, "file:///dir/file.html" },
	{ "-1; URL='file:///dir/file.html'",
	  0, -1UL, "file:///dir/file.html" },
	{ "-2; URL='file:///dir/file.html'",
	  0, -2UL, "file:///dir/file.html" },
	{ "garbage; URL='file:///dir/file.html'",
	  0, 0, "file:///dir/file.html" },
	{ "'5;1'; URL='file:///dir/file.html'",
	  0, 0, "1'; URL='file:///dir/file.html'" },
	{ "2,6; URL='file:///dir/file.html'",
	  0, 2, "6; URL='file:///dir/file.html'" },
	{ "2 3; URL='file:///dir/file.html'",
	  0, 2, "3; URL='file:///dir/file.html'" },

	/* unusual delay; not verified against Iceweasel */
	{ "9999999999999999999999999; URL='file:///dir/file.html'",
	  0, ULONG_MAX, "file:///dir/file.html" },

	/* other stuff after the URL; these match Iceweasel/3.5.16 */
	{ "5; URL=file:///dir/file.html   ",
	  0, 5, "file:///dir/file.html" },
	{ "5; URL=file:///dir/file.html\t",
	  0, 5, "file:///dir/file.html" },
	{ "5; URL=\"file:///dir/file.html\"  ",
	  0, 5, "file:///dir/file.html" },
	{ "5; URL=\"file:///dir/file.html\"\t\t",
	  0, 5, "file:///dir/file.html" },
	{ "5; URL=\"file:///dir/file.html\" ; ",
	  0, 5, "file:///dir/file.html" },
	{ "5; URL=\"file:///dir/file.html\"; transition=\"sweep\"",
	  0, 5, "file:///dir/file.html" },

	/* sentinel */
	{ NULL, 0, 0, NULL }
};

int
main(void)
{
	const struct meta_refresh_test_case *test;
	int count_ok = 0;
	int count_fail = 0;

	for (test = meta_refresh_test_cases; test->content; test++) {
		static unsigned char dummy[] = "dummy";
		unsigned long delay = 21;
		unsigned char *url = dummy;
		
		int error = html_parse_meta_refresh(test->content,
						    &delay, &url);
		if (error < 0 && test->error < 0 && url == NULL) {
			/* Test OK */
			count_ok++;
		} else if (error >= 0 && test->error >= 0
			   && ((!url && !test->url)
			       || (url && test->url && !strcmp(url, test->url)))
			   && delay == test->delay) {
			/* Test OK */
			count_ok++;
		} else {
			fprintf(stderr, "Test failed at input: %s\n"
				"\tParsed  error: %d\n"
				"\tCorrect error: %d\n"
				"\tParsed  delay: %lu\n"
				"\tCorrect delay: %lu\n"
				"\tParsed  URL: %s\n"
				"\tCorrect URL: %s\n",
				test->content,
				error,
				test->error,
				delay,
				test->delay,
				url ? (char *) url : "(null)",
				test->url ? (char *) test->url : "(null)");
			count_fail++;
		}

		if (url != dummy && url != NULL)
			mem_free(url);
	}

	printf("Summary of meta refresh tests: %d OK, %d failed.\n",
	       count_ok, count_fail);
	return count_fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
