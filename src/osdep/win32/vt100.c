
/* VT 100 emulation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#include "elinks.h"

#include "osdep/osdep.h"
#include "osdep/win32/win32.h"


int
VT100_decode(HANDLE fd, const void *b, int len)
{
	static unsigned char intensity = 0;
	static unsigned char fore = 7;
	static unsigned char back = 0;
	char *buf = (char *)b;

	int length = len;
	int i = 0;

	while (1) {
		if (len <= 0) return length;
		for (i = 0; i < len && buf[i] != 27; i++);
		if (i) {
			DWORD w;

			WriteConsole(fd, buf, i, &w, NULL);
			len -= w;
			if (len <= 0) return length;
			buf += w;
		}
		if (!strncmp(buf, "\033)0\0337", 5)) {
			buf += 5;
			len -= 5;
			continue;
		}
		if (!strncmp(buf, "\033[?47h", 6)) {
			buf += 6;
			len -= 6;
			continue;
		}
		if (!strncmp(buf, "\033[?47l", 6)) {
			buf += 6;
			len -= 6;
			continue;
		}
		if (!strncmp(buf, "\033[2J", 4)) {
			/* Clear screen */
			buf += 4;
			len -= 4;
			continue;
		}
		if (buf[1] == '[') {
			char *tail;
			char *begin;
			char *end;
			COORD pos;

			buf += 2;
			len -= 2;
			int k = strspn(buf, "0123456789;");
			if (!k) continue;
			switch (buf[k]) {
			case 'H':
				pos.Y = (short)strtol(buf, &tail, 10) - 1;
				pos.X = (short)strtol(tail + 1, NULL, 10) - 1;
				SetConsoleCursorPosition(fd, pos);
				break;
			case 'm':
				end = buf + k;
				begin = tail = buf;
				while (tail < end) {
					static unsigned char kolors[8] = {0, 4, 2, 6, 1, 5, 3, 7};
					unsigned char kod = (unsigned char)strtol(begin, &tail, 10);

					begin = tail + 1;
					if (kod == 0) {
						intensity = 0;
						back = 0;
						fore = 7;
					} else if (kod == 1) {
						intensity = 8;
					} else if (kod == 22) {
						intensity = 0;
					} else if (kod >= 30 && kod <= 37) {
						fore = kolors[kod - 30];
					} else if (kod >= 40 && kod <= 47) {
						back = kolors[kod - 40];
					}
				}
				SetConsoleTextAttribute(fd, fore | intensity | back << 4);
				break;
			default:
				break;
			}
			k++;
			buf += k;
			len -= k;
		} else {
			buf += 2;
			len -= 2;
		}
	}
}
