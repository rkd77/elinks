/* HTML manglizer
 * --------------
 * Copyright (C) 2004 by Michal Zalewski <lcamtuf@coredump.cx>
 *
 * Based on mangleme-1.2, downloaded on the date below.
 *
 * Date: Tue, 31 Jan 2006 10:14:38 +0100 (CET)
 * From: Michal Zalewski <lcamtuf@dione.ids.pl>
 * To: Jonas Fonseca <fonseca@diku.dk>
 * Subject: Re: [mangleme] Question about license and reusability
 * In-Reply-To: <20060131022616.GB30744@diku.dk>
 * Message-ID: <Pine.LNX.4.58.0601311013400.24339@dione>
 *
 * On Tue, 31 Jan 2006, Jonas Fonseca wrote:
 * > I would like to reuse your HTML manglizer for stress testing a program I
 * > am working on (licensed under GPL) but mangleme.tgz (version 1.2)
 * > doesn't carry any license info as far as I can see. The freshmeat
 * > project page mentions LGPL, is that authoritative?
 *
 * The program is so trivial I have no issues with other people using it
 * under any licence they choose, to be honest. Consider it to be freeware,
 * or LGPL if you insist on having some restrictions ;-)
 *
 * In other words, go ahead.
 *
 * Cheers,
 * /mz
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define R(x) (rand() % (x))

#define MAXTCOUNT 100
#define MAXPCOUNT 20
#define MAXSTR2   80
#define MAXTAGS 80
#define MAXPARS 20

/* Tag and parameter list: guesstimating / reference compilation. */
static char* tags[MAXTAGS][MAXPARS] = {
	{ "A", "NAME", "HREF", "REF", "REV", "TITLE", "TARGET", "SHAPE", "onLoad", "STYLE", 0 },
	{ "APPLET", "CODEBASE", "CODE", "NAME", "ALIGN", "ALT", "HEIGHT", "WIDTH", "HSPACE", "VSPACE", "DOWNLOAD", "HEIGHT", "NAME", "TITLE", "onLoad", "STYLE", 0 },
	{ "AREA", "SHAPE", "ALT", "CO-ORDS", "HREF", "onLoad", "STYLE", 0 },
	{ "B", "onLoad", "STYLE", 0 },
	{ "BANNER", "onLoad", "STYLE", 0 },
	{ "BASE", "HREF", "TARGET", "onLoad", "STYLE", 0 },
	{ "BASEFONT", "SIZE", "onLoad", "STYLE", 0 },
	{ "BGSOUND", "SRC", "LOOP", "onLoad", "STYLE", 0 },
	{ "BQ", "CLEAR", "NOWRAP", "onLoad", "STYLE", 0 },
	{ "BODY", "BACKGROUND", "BGCOLOR", "TEXT", "LINK", "ALINK", "VLINK", "LEFTMARGIN", "TOPMARGIN", "BGPROPERTIES", "onLoad", "STYLE", 0 },
	{ "CAPTION", "ALIGN", "VALIGN", "onLoad", "STYLE", 0 },
	{ "CENTER", "onLoad", "STYLE", 0 },
	{ "COL", "ALIGN", "SPAN", "onLoad", "STYLE", 0 },
	{ "COLGROUP", "ALIGN", "VALIGN", "HALIGN", "WIDTH", "SPAN", "onLoad", "STYLE", 0 },
	{ "DIV", "ALIGN", "CLASS", "LANG", "onLoad", "STYLE", 0 },
	{ "EMBED", "SRC", "HEIGHT", "WIDTH", "UNITS", "NAME", "PALETTE", "onLoad", "STYLE", 0 },
	{ "FIG", "SRC", "ALIGN", "HEIGHT", "WIDTH", "UNITS", "IMAGEMAP", "onLoad", "STYLE", 0 },
	{ "FN", "ID", "onLoad", "STYLE", 0 },
	{ "FONT", "SIZE", "COLOR", "FACE", "onLoad", "STYLE", 0 },
	{ "FORM", "ACTION", "METHOD", "ENCTYPE", "TARGET", "SCRIPT", "onLoad", "STYLE", 0 },
	{ "FRAME", "SRC", "NAME", "MARGINWIDTH", "MARGINHEIGHT", "SCROLLING", "FRAMESPACING", "onLoad", "STYLE", 0 },
	{ "FRAMESET", "ROWS", "COLS", "onLoad", "STYLE", 0 },
	{ "H1", "SRC", "DINGBAT", "onLoad", "STYLE", 0 },
	{ "HEAD", "onLoad", "STYLE", 0 },
	{ "HR", "SRC", "SIZE", "WIDTH", "ALIGN", "COLOR", "onLoad", "STYLE", 0 },
	{ "HTML", "onLoad", "STYLE", 0 },
	{ "IFRAME", "ALIGN", "FRAMEBORDER", "HEIGHT", "MARGINHEIGHT", "MARGINWIDTH", "NAME", "SCROLLING", "SRC", "ADDRESS", "WIDTH", "onLoad", "STYLE", 0 },
	{ "IMG", "ALIGN", "ALT", "SRC", "BORDER", "DYNSRC", "HEIGHT", "HSPACE", "ISMAP", "LOOP", "LOWSRC", "START", "UNITS", "USEMAP", "WIDTH", "VSPACE", "onLoad", "STYLE", 0 },
	{ "INPUT", "TYPE", "NAME", "VALUE", "onLoad", "STYLE", 0 },
	{ "ISINDEX", "HREF", "PROMPT", "onLoad", "STYLE", 0 },
	{ "LI", "SRC", "DINGBAT", "SKIP", "TYPE", "VALUE", "onLoad", "STYLE", 0 },
	{ "LINK", "REL", "REV", "HREF", "TITLE", "onLoad", "STYLE", 0 },
	{ "MAP", "NAME", "onLoad", "STYLE", 0 },
	{ "MARQUEE", "ALIGN", "BEHAVIOR", "BGCOLOR", "DIRECTION", "HEIGHT", "HSPACE", "LOOP", "SCROLLAMOUNT", "SCROLLDELAY", "WIDTH", "VSPACE", "onLoad", "STYLE", 0 },
	{ "MENU", "onLoad", "STYLE", 0 },
	{ "META", "HTTP-EQUIV", "CONTENT", "NAME", "onLoad", "STYLE", 0 },
	{ "MULTICOL", "COLS", "GUTTER", "WIDTH", "onLoad", "STYLE", 0 },
	{ "NOFRAMES", "onLoad", "STYLE", 0 },
	{ "NOTE", "CLASS", "SRC", "onLoad", "STYLE", 0 },
	{ "OVERLAY", "SRC", "X", "Y", "HEIGHT", "WIDTH", "UNITS", "IMAGEMAP", "onLoad", "STYLE", 0 },
	{ "PARAM", "NAME", "VALUE", "onLoad", "STYLE", 0 },
	{ "RANGE", "FROM", "UNTIL", "onLoad", "STYLE", 0 },
	{ "SCRIPT", "LANGUAGE", "onLoad", "STYLE", 0 },
	{ "SELECT", "NAME", "SIZE", "MULTIPLE", "WIDTH", "HEIGHT", "UNITS", "onLoad", "STYLE", 0 },
	{ "OPTION", "VALUE", "SHAPE", "onLoad", "STYLE", 0 },
	{ "SPACER", "TYPE", "SIZE", "WIDTH", "HEIGHT", "ALIGN", "onLoad", "STYLE", 0 },
	{ "SPOT", "ID", "onLoad", "STYLE", 0 },
	{ "TAB", "INDENT", "TO", "ALIGN", "DP", "onLoad", "STYLE", 0 },
	{ "TABLE", "ALIGN", "WIDTH", "BORDER", "CELLPADDING", "CELLSPACING", "BGCOLOR", "VALIGN", "COLSPEC", "UNITS", "DP", "onLoad", "STYLE", 0 },
	{ "TBODY", "CLASS", "ID", "onLoad", "STYLE", 0 },
	{ "TD", "COLSPAN", "ROWSPAN", "ALIGN", "VALIGN", "BGCOLOR", "onLoad", "STYLE", 0 },
	{ "TEXTAREA", "NAME", "COLS", "ROWS", "onLoad", "STYLE", 0 },
	{ "TEXTFLOW", "CLASS", "ID", "onLoad", "STYLE", 0 },
	{ "TFOOT", "COLSPAN", "ROWSPAN", "ALIGN", "VALIGN", "BGCOLOR", "onLoad", "STYLE", 0 },
	{ "TH", "ALIGN", "CLASS", "ID", "onLoad", "STYLE", 0 },
	{ "TITLE", "onLoad", "STYLE", 0 },
	{ "TR", "ALIGN", "VALIGN", "BGCOLOR", "CLASS", "onLoad", "STYLE", 0 },
	{ "UL", "SRC", "DINGBAT", "WRAP", "TYPE", "PLAIN", "onLoad", "STYLE", 0 },
	{ 0 }
};

void make_up_value(void)
{
	char c=R(2);

	if (c) putchar('"');

	switch (R(31)) {
	case 0: printf("javascript:"); make_up_value(); break;
#if 0
	case 1: printf("jar:"); make_up_value(); break;
#endif
	case 2: printf("mk:"); make_up_value(); break;
	case 3: printf("file:"); make_up_value(); break;
	case 4: printf("http:"); make_up_value(); break;
	case 5: printf("about:"); make_up_value(); break;
	case 6: printf("_blank"); break;
	case 7: printf("_self"); break;
	case 8: printf("top"); break;
	case 9: printf("left"); break;
	case 10: putchar('&'); make_up_value(); putchar(';'); break;
	case 11: make_up_value(); make_up_value(); break;
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
	{
		int c = R(10) ? R(10) : (1 + R(MAXSTR2) * R(MAXSTR2));
		char *x = malloc(c);

		memset(x,R(256),c);
		fwrite(x,c,1,stdout);
		free(x);
		break;
	}
	case 21: printf("%s","%n%n%n%n%n%n"); break;
	case 22: putchar('#'); break;
	case 23: putchar('*'); break;
	default:
		 if (R(2)) putchar('-'); printf("%d",rand());
		 break;
	}

	if (c) putchar('"');
}

void random_tag(void)
{
	int tn, tc;

	do
		tn = R(MAXTAGS);
	while (!tags[tn][0]);

	tc = R(MAXPCOUNT) + 1;

	putchar('<');

	switch (R(10)) {
	case 0: putchar(R(256)); break;
	case 1: putchar('/');
	}

	printf("%s", tags[tn][0]);

	while (tc--) {
		int pn;

		switch (R(32)) {
		case 0: putchar(R(256));
		case 1: break;
		default: putchar(' ');
		}

		do
			pn = R(MAXPARS-1) + 1;
		while (!tags[tn][pn]);

		printf("%s", tags[tn][pn]);

		switch (R(32)) {
		case 0: putchar(R(256));
		case 1: break;
		default: putchar('=');
		}

		make_up_value();
	}

	putchar('>');

}

int main(int argc, char **argv)
{
	int tc, seed;

	printf("<HTML>\n");
	printf("<HEAD>\n");

	seed = (time(0) ^ (getpid() << 16));
#if 0
	fprintf(stderr,"[%u] Mangle attempt 0x%08x (%s) -- %s\n", (int)time(0), seed, getenv("HTTP_USER_AGENT"), getenv("REMOTE_ADDR"));
#endif
	srand(seed);

	tc = R(MAXTCOUNT) + 1;
	while (tc--) random_tag();
	fflush(0);
	return 0;
}
