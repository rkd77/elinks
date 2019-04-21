/* Test match_hostname_pattern() */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "network/ssl/match-hostname.h"
#include "util/string.h"

struct match_hostname_pattern_test_case
{
	const unsigned char *pattern;
	const unsigned char *hostname;
	int match;
};

static const struct match_hostname_pattern_test_case match_hostname_pattern_test_cases[] = {
	{ "*r*.example.org", "random.example.org", 1 },
	{ "*r*.example.org", "history.example.org", 1 },
	{ "*r*.example.org", "frozen.fruit.example.org", 0 },
	{ "*r*.example.org", "steamed.fruit.example.org", 0 },

	{ "ABC.def.Ghi", "abc.DEF.gHI", 1 },

	{ "*", "localhost", 1 },
	{ "*", "example.org", 0 },
	{ "*.*", "example.org", 1 },
	{ "*.*.*", "www.example.org", 1 },
	{ "*.*.*", "example.org", 0 },

	{ "assign", "assignee", 0 },
	{ "*peg", "arpeggiator", 0 },
	{ "*peg*", "arpeggiator", 1 },
	{ "*r*gi*", "arpeggiator", 1 },
	{ "*r*git*", "arpeggiator", 0 },

	{ NULL, NULL, 0 }
};

int
main(void)
{
	const struct match_hostname_pattern_test_case *test;
	int count_ok = 0;
	int count_fail = 0;
	struct string hostname_str = NULL_STRING;
	struct string pattern_str = NULL_STRING;

	if (!init_string(&hostname_str) || !init_string(&pattern_str)) {
		fputs("Out of memory.\n", stderr);
		done_string(&hostname_str);
		done_string(&pattern_str);
		return EXIT_FAILURE;
	}

	for (test = match_hostname_pattern_test_cases; test->pattern; test++) {
		int match;

		match = match_hostname_pattern(
			test->hostname,
			strlen(test->hostname),
			test->pattern,
			strlen(test->pattern));
		if (!match == !test->match) {
			/* Test OK */
			count_ok++;
		} else {
			fprintf(stderr, "match_hostname_pattern() test failed\n"
				"\tHostname: %s\n"
				"\tPattern: %s\n"
				"\tActual result: %d\n"
				"\tCorrect result: %d\n",
				test->hostname,
				test->pattern,
				match,
				test->match);
			count_fail++;
		}

		/* Try with strings that are not null-terminated.  */
		hostname_str.length = 0;
		add_to_string(&hostname_str, test->hostname);
		add_to_string(&hostname_str, "ZZZ");
		pattern_str.length = 0;
		add_to_string(&pattern_str, test->pattern);
		add_to_string(&hostname_str, "______");

		match = match_hostname_pattern(
			hostname_str.source,
			strlen(test->hostname),
			pattern_str.source,
			strlen(test->pattern));
		if (!match == !test->match) {
			/* Test OK */
			count_ok++;
		} else {
			fprintf(stderr, "match_hostname_pattern() test failed\n"
				"\tVariant: Strings were not null-terminated.\n"
				"\tHostname: %s\n"
				"\tPattern: %s\n"
				"\tActual result: %d\n"
				"\tCorrect result: %d\n",
				test->hostname,
				test->pattern,
				match,
				test->match);
			count_fail++;
		}
	}

	printf("Summary of match_hostname_pattern() tests: %d OK, %d failed.\n",
	       count_ok, count_fail);

	done_string(&hostname_str);
	done_string(&pattern_str);
	return count_fail ? EXIT_FAILURE : EXIT_SUCCESS;
	
}
