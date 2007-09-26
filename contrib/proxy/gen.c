#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct stat st;
char *file = "/tmp/log";
unsigned char *data;
int counter = 0;

struct {
	unsigned char *string;
	unsigned char *host;
	unsigned char *data;
	int length;
} tab[100000]; /* should be enough */

unsigned char header[] =
"#!/usr/bin/env python\n"
"import BaseHTTPServer\n\n";

unsigned char footer[] =
"class Serwer(BaseHTTPServer.BaseHTTPRequestHandler):\n"
"\tdef do_GET(self):\n"
"\t\tglobal slownik\n"
"\t\tof = open(slownik[self.path])\n"
"\t\tprint (self.path)\n"
"\t\tself.wfile.write(of.read())\n"
"\t\tof.close()\n\n"
"\tdef do_POST(self):\n"
"\t\tself.do_GET()\n\n"
"def run(server_class = BaseHTTPServer.HTTPServer, handler_class = Serwer):\n"
"\tserver_address = ('', 8000)\n"
"\thttpd = server_class(server_address, handler_class)\n"
"\thttpd.serve_forever()\n\n"
"run()\n";

static unsigned char *
find(unsigned char *from, unsigned char *key)
{
	unsigned char *end = data + st.st_size;
	unsigned char *beg;
	int l = strlen(key);

	for (beg = from;; beg++) {
		beg = memchr(beg, key[0], end - beg);
		if (!beg)
			break;
		if (!strncmp(beg, key, l))
			break;
	}
	return beg;
}

static void
parse(void)
{
	unsigned char *current = data;

	while (1) {
		unsigned char *conn = find(current, "CONNECTION:");
		unsigned char *get = find(current, "GET /");
		unsigned char *post = find(current, "POST /");
		unsigned char *host, *space, *http;
		size_t enter;

		if (counter) {
			unsigned char *min = data + st.st_size;

			if (conn)
				min = conn;
			if (get && get < min)
				min = get;
			if (post && post < min)
				min = post;
			tab[counter - 1].length = min - http;
		}
		if (get && post) {
			if (get < post) {
				current = get + 4;
			} else {
				current = post + 5;
			}
		} else {
			if (get) {
				current = get + 4;
			} else if (post) {
				current = post + 5;
			} else {
				return;
			}
		}

		space = strchr(current, ' ');
		if (!space)
			return;

		host = find(space + 1, "Host: ");
		if (!host)
			return;

		*space = '\0';
		tab[counter].string = current;
		host += 6;
		enter = strcspn(host, "\r\n\0");
		host[enter] = '\0';
		tab[counter].host = host;

		http = find(host + enter + 1, "HTTP/");
		if (!http)
			return;
		tab[counter++].data = http;
	}
}

static void
dicts(FILE *f)
{
	int i;

	fprintf(f, "slownik = {\n");
	for (i = 0; i < counter - 1; i++) {
		fprintf(f, "\t'http://%s%s' : '%d.http',\n", tab[i].host, tab[i].string, i);
	}
	for (; i < counter; i++) {
		fprintf(f, "\t'http://%s%s' : '%d.http'\n", tab[i].host, tab[i].string, i);
	}
	fprintf(f, "}\n\n");
}

static void
save(void)
{
	int i;
	FILE *f;

	for (i = 0; i < counter; i++) {
		char buf[12];

		snprintf(buf, 12, "%d.http", i);
		f = fopen(buf, "w");
		if (!f)
			return;
		fwrite(tab[i].data, 1, tab[i].length, f);
		fclose(f);
	}
	f = fopen("proxy.py", "w");
	if (!f)
		return;
	fprintf(f, "%s", header);
	dicts(f);
	fprintf(f, "%s", footer);
	fclose(f);
}

int
main(int argc, char **argv)
{
	FILE *f;

	if (argc > 1)
		file = argv[1];
	f = fopen(file, "r");
	if (!f)
		return 1;
	stat(file, &st);
	data = calloc(1, st.st_size + 1);
	if (!data)
		return 2;
	fread(data, 1, st.st_size, f);
	fclose(f);

	parse();
	save();
	return 0;
}
