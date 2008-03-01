/* Parser of HTTP date */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#ifdef TIME_WITH_SYS_TIME
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#else
#if defined(TM_IN_SYS_TIME) && defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#elif defined(HAVE_TIME_H)
#include <time.h>
#endif
#endif

#include "elinks.h"

#include "protocol/date.h"
#include "util/conv.h"
#include "util/time.h"

/*
 * Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 * Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 * Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 */

int
parse_year(const unsigned char **date_p, unsigned char *end)
{
	const unsigned char *date = *date_p;
	int year;

	if ((end && date + 1 >= end)
	    || !isdigit(date[0])
	    || !isdigit(date[1]))
		return -1;

	year = (date[0] - '0') * 10 + date[1] - '0';

	if ((!end || date + 3 < end)
	    && isdigit(date[2])
	    && isdigit(date[3])) {
		/* Four digits date */
		year = year * 10 + date[2] - '0';
		year = year * 10 + date[3] - '0' - 1900;
		date += 4;

	} else if (year < 70) {
		/* Assuming the epoch starting at 1.1.1970 so it's already next
		 * century. --wget */
		year += 100;
		date += 2;
	}

	*date_p = date;
	return year;
}

int
parse_month(const unsigned char **buf, unsigned char *end)
{
	const unsigned char *month = *buf;
	int monthnum;

	/* Check that the buffer has atleast 3 chars. */
	if ((end && month + 2 > end)
	    || !month[0] || !month[1] || !month[2])
		return -1;

	monthnum = month2num(month);

	if (monthnum != -1)
		*buf += 3;

	return monthnum;
}


/* Return day number. */
int
parse_day(const unsigned char **date_p, unsigned char *end)
{
	const unsigned char *date = *date_p;
	int day;

	if ((end && date >= end) || !isdigit(*date))
		return 32;
	day = *date++ - '0';

	if ((!end || date < end) && isdigit(*date)) {
		day = day * 10 + *date++ - '0';
	}

	*date_p = date;
	return day;
}

int
parse_time(const unsigned char **time, struct tm *tm, unsigned char *end)
{
	unsigned char h1, h2, m1, m2;
	const unsigned char *date = *time;

#define check_time(tm) \
	((tm)->tm_hour <= 23 && (tm)->tm_min <= 59 && (tm)->tm_sec <= 59)

	/* Eat HH:MM */
	if (end && date + 5 > end)
		return 0;

	h1 = *date++; if (!isdigit(h1)) return 0;
	h2 = *date++; if (!isdigit(h2)) return 0;

	if (*date++ != ':') return 0;

	m1 = *date++; if (!isdigit(m1)) return 0;
	m2 = *date++; if (!isdigit(m2)) return 0;

	tm->tm_hour = (h1 - '0') * 10 + h2 - '0';
	tm->tm_min  = (m1 - '0') * 10 + m2 - '0';
	tm->tm_sec = 0;

	/* Eat :SS or [PA]M or nothing */
	if (end && date + 2 >= end) {
		*time = date;
		return check_time(tm);
	}

	if (*date == ':') {
		unsigned char s1, s2;

		date++;

		if (end && date + 2 >= end)
			return 0;

		s1 = *date++; if (!isdigit(s1)) return 0;
		s2 = *date++; if (!isdigit(s2)) return 0;

		tm->tm_sec = (s1 - '0') * 10 + s2 - '0';

	} else if (*date == 'A' || *date == 'P') {
		/* Adjust hour from AM/PM. The sequence goes 11:00AM, 12:00PM,
		 * 01:00PM ... 11:00PM, 12:00AM, 01:00AM. --wget */
		if (tm->tm_hour == 12)
			tm->tm_hour = 0;

		if (*date++ == 'P')
			tm->tm_hour += 12;

		if (*date++ != 'M')
			return 0;
	}

	*time = date;

	return check_time(tm);
}


static time_t
my_timegm(struct tm *tm)
{
	time_t t = 0;

	/* Okay, the next part of the code is somehow problematic. Now, we use
	 * own code for calculating the number of seconds from 1.1.1970,
	 * brought here by SC from w3m. I don't like it a lot, but it's 100%
	 * portable, it's faster and it's shorter. --pasky */
#if 0
#ifdef HAVE_TIMEGM
	t = timegm(tm);
#else
	/* Since mktime thinks we have localtime, we need a wrapper
	 * to handle GMT. */
	/* FIXME: It was reported that it doesn't work somewhere :/. */
	{
		unsigned char *tz = getenv("TZ");

		if (tz && *tz) {
			/* Temporary disable timezone in-place. */
			unsigned char tmp = *tz;

			*tz = '\0';
			tzset();

			t = mktime(tm);

			*tz = tmp;
			tzset();

		} else {
			/* Already GMT, cool! */
			t = mktime(tm);
		}
	}
#endif
#else
	/* Following code was borrowed from w3m, and its developers probably
	 * borrowed it from somewhere else as well, altough they didn't bother
	 * to mention that. */ /* Actually, same code appears to be in lynx as
	 * well.. oh well. :) */
	/* See README.timegm for explanation about how this works. */
	tm->tm_mon -= 2;
	if (tm->tm_mon < 0) {
		tm->tm_mon += 12;
		tm->tm_year--;
	}
	tm->tm_mon *= 153; tm->tm_mon += 2;
	tm->tm_year -= 68;

	/* Bug 924: Assumes all years divisible by 4 are leap years,
	 * even though e.g. 2100 is not.  */
	tm->tm_mday += tm->tm_year * 1461 / 4;
	tm->tm_mday += ((tm->tm_mon / 5) - 672);

	t = ((tm->tm_mday * 60 * 60 * 24) +
	     (tm->tm_hour * 60 * 60) +
	     (tm->tm_min * 60) +
	     tm->tm_sec);
#endif

	if (t == (time_t) -1) return 0;

	return t;
}


time_t
parse_date(unsigned char **date_pos, unsigned char *end,
	   int update_pos, int skip_week_day)
{
#define skip_time_sep(date, end) \
	do { \
		const unsigned char *start = (date); \
		while ((!(end) || (date) < (end)) \
			&& (*(date) == ' ' || *(date) == '-')) \
			(date)++; \
		if (date == start) return 0; \
	} while (0)

	struct tm tm;
	const unsigned char *date = (const unsigned char *) *date_pos;

	if (!date) return 0;

	if (skip_week_day) {
		/* Skip day-of-week */
		for (; (!end || date < end) && *date != ' '; date++)
			if (!*date) return 0;

		/* As pasky said who cares if we allow '-'s here? */
		skip_time_sep(date, end);
	}

	if (isdigit(*date)) {
		/* RFC 1036 / RFC 1123 */

		/* Eat day */

		/* date++; */
		tm.tm_mday = parse_day(&date, end);
		if (tm.tm_mday > 31) return 0;

		skip_time_sep(date, end);

		/* Eat month */

		tm.tm_mon = parse_month(&date, end);
		if (tm.tm_mon < 0) return 0;

		skip_time_sep(date, end);

		/* Eat year */

		tm.tm_year = parse_year(&date, end);
		if (tm.tm_year < 0) return 0;

		skip_time_sep(date, end);

		/* Eat time */

		if (!parse_time(&date, &tm, end)) return 0;

	} else {
		/* ANSI C's asctime() format */

		/* Eat month */

		tm.tm_mon = parse_month(&date, end);
		if (tm.tm_mon < 0) return 0;

		/* I know, we shouldn't allow '-', but who cares ;). --pasky */
		skip_time_sep(date, end);

		/* Eat day */

		tm.tm_mday = parse_day(&date, end);
		if (tm.tm_mday > 31) return 0;

		skip_time_sep(date, end);

		/* Eat time */

		if (!parse_time(&date, &tm, end)) return 0;

		skip_time_sep(date, end);

		/* Eat year */

		tm.tm_year = parse_year(&date, end);
		if (tm.tm_year < 0) return 0;
	}
#undef skip_time_sep

	if (update_pos)
		*date_pos = (unsigned char *) date;

	return (time_t) my_timegm(&tm);
}
