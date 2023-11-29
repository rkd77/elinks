#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	int i,j;
#if 0
	FILE *log = fopen("/dev/shm/log", "a");
	fprintf(log, "Before:\n");
	for (i = 0; i <= argc; i++) {
		fprintf(log, "argv[%d]=%s\n", i, argv[i]);
	}
	fprintf(log, "----\n");
	fclose(log);
#endif
	argv[0] = "tcc";

	if (argc > 1) {
		if (!strcmp(argv[1], "--version")) {
			argv[0] = "gcc";
		} else if (!strcmp(argv[1], "-Wl,--version")) {
			argv[0] = "gcc";
		} else {
			for (i = 1, j = 1; i < argc;) {
				if (!strcmp(argv[j], "-MQ") || !strcmp(argv[j], "-MF")) {
					j += 2;
					argc -= 2;
				} else if (!strcmp(argv[j], "-Wl,--no-undefined") || !strcmp(argv[j], "-Wl,--start-group") || !strcmp(argv[j], "-Wl,--end-group")
					|| !strncmp(argv[j], "-Wl,-rpath-link,", sizeof("-Wl,-rpath-link,") - 1)) {
					++j;
					--argc;
				} else {
					argv[i++] = argv[j++];
				}
			}
			argv[i] = argv[j];
		}
	}
#if 0
	log = fopen("/dev/shm/log", "a");
	fprintf(log, "After\n");
	for (i = 0; i <= argc; i++) {
		fprintf(log, "argv[%d]=%s\n", i, argv[i]);
	}
	fprintf(log, "----\n");
	fclose(log);
#endif
	return execvp(argv[0], argv);
}
