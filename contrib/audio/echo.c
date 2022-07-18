#include <stdio.h>

int
main(int argc, char **argv)
{
    printf("{\"command\":[\"loadfile\",\"%s\",\"append-play\"]}\n", argv[1]);
    return 0;
}
