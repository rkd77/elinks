#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/charsets.h"
#include "util/test.h"

enum direction {
	DIRECTION_FORWARD,
	DIRECTION_BACKWARD
};

static enum direction
parse_direction(const char *name)
{
	if (strcmp(name, "forward") == 0)
		return DIRECTION_FORWARD;
	else if (strcmp(name, "backward") == 0)
		return DIRECTION_BACKWARD;
	else
		die("direction must be forward or backward, not %s\n", name);
}

static enum utf8_step
parse_way(const char *name)
{
	if (strcmp(name, "characters") == 0)
		return UTF8_STEP_CHARACTERS;
	else if (strcmp(name, "cells-fewer") == 0)
		return UTF8_STEP_CELLS_FEWER;
	else if (strcmp(name, "cells-more") == 0)
		return UTF8_STEP_CELLS_MORE;
	else
		die("way must be characters, cells-fewer, or cells-more; not %s\n", name);
}

int
main(int argc, char **argv)
{
	enum direction direction;
	enum utf8_step way;
	int max_steps;
	unsigned char *begin, *end;
	int expect_bytes;
	int expect_steps;
	int bytes;
	int steps;

	if (argc != 7)
		die("usage: %s (forward|backward) (characters|cells-fewer|cells-more) \\\n"
		    "\tMAX-STEPS STRING EXPECT-BYTES EXPECT-STEPS\n",
		    argc ? argv[0] : "utf8-step-test");

	direction = parse_direction(argv[1]);
	way = parse_way(argv[2]);
	max_steps = atoi(argv[3]);
	begin = argv[4];
	end = begin + strlen(begin);
	expect_bytes = atoi(argv[5]);
	expect_steps = atoi(argv[6]);

	switch (direction) {
	case DIRECTION_FORWARD:
		bytes = utf8_step_forward(begin, end, max_steps, way, &steps) - begin;
		break;
	case DIRECTION_BACKWARD:
		bytes = end - utf8_step_backward(end, begin, max_steps, way, &steps);
		break;
	default:
		abort();
	}

	if (bytes != expect_bytes || steps != expect_steps)
		die("FAIL: bytes=%d (expected %d); steps=%d (expected %d)\n",
		    bytes, expect_bytes, steps, expect_steps);

	return 0;
}
