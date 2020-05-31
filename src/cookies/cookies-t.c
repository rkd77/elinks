/* Tool for testing the cookies path  */

#include <stdio.h>
#include "path.h"

int main(int argc, char **argv)
{
	int res = is_path_prefix(argv[1], argv[2]);
	printf("is_path_prefix(\"%s\", \"%s\")=%d\n", argv[1], argv[2], res);

	return !res;
}
