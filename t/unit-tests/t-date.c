#include "test-lib.h"
#include "date.h"
#include "strbuf.h"

/* Reference time: 2009-08-30 19:20:00. */
#define GIT_TEST_DATE_NOW 1251660000

/* The time corresponds to Wed, 15 Jun 2016 16:13:20 +0200. */
static const char test_time[] = "1466000000 +0200";

enum prerequisites {
    	TIME_IS_64BIT = 1 << 0,
    	TIME_T_IS_64BIT = 1 << 1,
};

/* Macro to check prerequisites */
#define CHECK_PREREQ(var, prereq) \
    	do { \
		if ((var) & prereq && !check_prereq_##prereq()) { \
			test_skip("missing prerequisite " #prereq); \
			return 0; \
		} \
	} while (0)

/* Return 1 if all prereqs are satisfied, 0 otherwise */
static int check_prereqs(unsigned int prereqs) {
    	CHECK_PREREQ(prereqs, TIME_IS_64BIT);
    	CHECK_PREREQ(prereqs, TIME_T_IS_64BIT);

    	return 1;
}

static void set_TZ_env(const char *zone) {
	setenv("TZ", zone, 1);
	tzset();
}

static void check_relative_dates(int time_val, const char *expected_date) {
	struct strbuf buf = STRBUF_INIT;
	timestamp_t diff = GIT_TEST_DATE_NOW - time_val;

	show_date_relative(diff, &buf);
	check_str(buf.buf, expected_date);
	strbuf_release(&buf);
}

#define TEST_RELATIVE_DATE(value, expected_output) \
    	TEST(check_relative_dates(value, expected_output), \
        	"relative date (%s) works", #expected_output )

static void check_show_date(const char *format, const char *TIME, const char *expected, unsigned int prereqs, const char *zone) {
	struct date_mode mode = DATE_MODE_INIT;
	char *arg;
	timestamp_t t;
	int tz;

	if (!check_prereqs(prereqs))
		return;
	if (strcmp(zone, ""))
		set_TZ_env(zone);

	parse_date_format(format, &mode);
	t = parse_timestamp(TIME, &arg, 10);
	tz = atoi(arg);

	check_str(show_date(t, tz, &mode), expected);

	if (strcmp(zone, ""))
		set_TZ_env("UTC");
	date_mode_release(&mode);
}

#define TEST_SHOW_DATE(format, time, expected, prereqs, zone) \
	TEST(check_show_date(format, time, expected, prereqs, zone), \
	     "show date (%s) works", #format)

static void check_parse_date(const char *given, const char *expected, const char *zone) {
	struct strbuf result = STRBUF_INIT;
	timestamp_t t;
	int tz;

	if (strcmp(zone, ""))
		set_TZ_env(zone);

	parse_date(given, &result);
	if (sscanf(result.buf, "%"PRItime" %d", &t, &tz) == 2)
		check_str(show_date(t, tz, DATE_MODE(ISO8601)), expected);
	else
		check_str("bad", expected);

	if (strcmp(zone, ""))
		set_TZ_env("UTC");
	strbuf_release(&result);
}

#define TEST_PARSE_DATE(given, expected_output, zone) \
    	TEST(check_parse_date(given, expected_output, zone), \
        	"parse date (%s) works", #expected_output)

static void check_approxidate(const char *given, const char *expected) {
	timestamp_t t = approxidate(given);
	char *expected_with_offset = xstrfmt("%s +0000", expected);

	check_str(show_date(t, 0, DATE_MODE(ISO8601)), expected_with_offset);
	free(expected_with_offset);
}

#define TEST_APPROXIDATE(given, expected_output) \
    	TEST(check_approxidate(given, expected_output), \
        	"parse approxidate (%s) works", #given)

static void check_date_format_human(int given, const char *expected) {
	timestamp_t diff = GIT_TEST_DATE_NOW - given;
	check_str(show_date(diff, 0, DATE_MODE(HUMAN)), expected);
}

#define TEST_DATE_FORMAT_HUMAN(given, expected_output) \
    	TEST(check_date_format_human(given, expected_output), \
        	"human date (%s) works", #given)

int cmd_main(int argc, const char **argv) {
	set_TZ_env("UTC");
	setenv("GIT_TEST_DATE_NOW", "1251660000", 1);
	setenv("LANG", "C", 1);

	TEST_RELATIVE_DATE(5, "5 seconds ago");
	TEST_RELATIVE_DATE(300, "5 minutes ago");
	TEST_RELATIVE_DATE(18000, "5 hours ago");
	TEST_RELATIVE_DATE(432000, "5 days ago");
	TEST_RELATIVE_DATE(1728000, "3 weeks ago");
	TEST_RELATIVE_DATE(13000000, "5 months ago");
	TEST_RELATIVE_DATE(37500000, "1 year, 2 months ago");
	TEST_RELATIVE_DATE(55188000, "1 year, 9 months ago");
	TEST_RELATIVE_DATE(630000000, "20 years ago");
	TEST_RELATIVE_DATE(31449600, "12 months ago");
	TEST_RELATIVE_DATE(62985600, "2 years ago");

	TEST_SHOW_DATE("iso8601", test_time, "2016-06-15 16:13:20 +0200", 0, "");
	TEST_SHOW_DATE("iso8601-strict", test_time, "2016-06-15T16:13:20+02:00", 0, "");
	TEST_SHOW_DATE("rfc2822", test_time, "Wed, 15 Jun 2016 16:13:20 +0200", 0, "");
	TEST_SHOW_DATE("short", test_time, "2016-06-15", 0, "");
	TEST_SHOW_DATE("default", test_time, "Wed Jun 15 16:13:20 2016 +0200", 0, "");
	TEST_SHOW_DATE("raw", test_time, test_time, 0, "");
	TEST_SHOW_DATE("unix", test_time, "1466000000", 0, "");
	TEST_SHOW_DATE("iso-local", test_time, "2016-06-15 14:13:20 +0000", 0, "");
	TEST_SHOW_DATE("raw-local", test_time, "1466000000 +0000", 0, "");
	TEST_SHOW_DATE("unix-local", test_time, "1466000000", 0, "");

	TEST_SHOW_DATE("format:%z", test_time, "+0200", 0, "");
	TEST_SHOW_DATE("format-local:%z", test_time, "+0000", 0, "");
	TEST_SHOW_DATE("format:%Z", test_time, "", 0, "");
	TEST_SHOW_DATE("format-local:%Z", test_time, "UTC", 0, "");
	TEST_SHOW_DATE("format:%%z", test_time, "%z", 0, "");
	TEST_SHOW_DATE("format-local:%%z", test_time, "%z", 0, "");

	TEST_SHOW_DATE("format:%Y-%m-%d %H:%M:%S", test_time, "2016-06-15 16:13:20", 0, "");

	TEST_SHOW_DATE("format-local:%Y-%m-%d %H:%M:%S", test_time, "2016-06-15 09:13:20", 0, "EST5");

	TEST_SHOW_DATE("format:%s", "123456789 +1234", "123456789", 0, "");
	TEST_SHOW_DATE("format:%s", "123456789 -1234", "123456789", 0, "");
	TEST_SHOW_DATE("format-local:%s", "123456789 -1234", "123456789", 0, "");

	/* Arbitrary time absurdly far in the future */
	TEST_SHOW_DATE("iso", "5758122296 -0400", "2152-06-19 18:24:56 -0400", TIME_IS_64BIT | TIME_T_IS_64BIT, "");
	TEST_SHOW_DATE("iso-local", "5758122296 -0400", "2152-06-19 22:24:56 +0000", TIME_IS_64BIT | TIME_T_IS_64BIT, "");

	TEST_PARSE_DATE("2000", "bad", "");
	TEST_PARSE_DATE("2008-02", "bad", "");
	TEST_PARSE_DATE("2008-02-14", "bad", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45", "2008-02-14 20:30:45 +0000", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -0500", "2008-02-14 20:30:45 -0500", "");
	TEST_PARSE_DATE("2008.02.14 20:30:45 -0500", "2008-02-14 20:30:45 -0500", "");
	TEST_PARSE_DATE("20080214T20:30:45", "2008-02-14 20:30:45 +0000", "");
	TEST_PARSE_DATE("20080214T20:30", "2008-02-14 20:30:00 +0000", "");
	TEST_PARSE_DATE("20080214T20", "2008-02-14 20:00:00 +0000", "");
	TEST_PARSE_DATE("20080214T203045", "2008-02-14 20:30:45 +0000", "");
	TEST_PARSE_DATE("20080214T2030", "2008-02-14 20:30:00 +0000", "");
	TEST_PARSE_DATE("20080214T000000.20", "2008-02-14 00:00:00 +0000", "");
	TEST_PARSE_DATE("20080214T00:00:00.20", "2008-02-14 00:00:00 +0000", "");
	TEST_PARSE_DATE("20080214T203045-04:00", "2008-02-14 20:30:45 -0400", "");

	TEST_PARSE_DATE("20080214T203045 -04:00", "2008-02-14 20:30:45 -0400", "");
	TEST_PARSE_DATE("20080214T203045.019-04:00", "2008-02-14 20:30:45 -0400", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45.019-04:00", "2008-02-14 20:30:45 -0400", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -0015", "2008-02-14 20:30:45 -0015", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -5", "2008-02-14 20:30:45 +0000", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -5:", "2008-02-14 20:30:45 +0000", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -05", "2008-02-14 20:30:45 -0500", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -:30", "2008-02-14 20:30:45 +0000", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45 -05:00", "2008-02-14 20:30:45 -0500", "");
	TEST_PARSE_DATE("2008-02-14 20:30:45", "2008-02-14 20:30:45 -0500", "EST5");

	TEST_PARSE_DATE("Thu, 7 Apr 2005 15:14:13 -0700", "2005-04-07 15:14:13 -0700", "");

	TEST_APPROXIDATE("now", "2009-08-30 19:20:00");
	TEST_APPROXIDATE("5 seconds ago", "2009-08-30 19:19:55");
	TEST_APPROXIDATE("10 minutes ago", "2009-08-30 19:10:00");
	TEST_APPROXIDATE("yesterday", "2009-08-29 19:20:00");
	TEST_APPROXIDATE("3 days ago", "2009-08-27 19:20:00");
	TEST_APPROXIDATE("12:34:56.3 days ago", "2009-08-27 12:34:56");
	TEST_APPROXIDATE("3 weeks ago", "2009-08-09 19:20:00");
	TEST_APPROXIDATE("3 months ago", "2009-05-30 19:20:00");
	TEST_APPROXIDATE("2 years 3 months ago", "2007-05-30 19:20:00");

	TEST_APPROXIDATE("6am yesterday", "2009-08-29 06:00:00");
	TEST_APPROXIDATE("6pm yesterday", "2009-08-29 18:00:00");
	TEST_APPROXIDATE("3:00", "2009-08-30 03:00:00");
	TEST_APPROXIDATE("15:00", "2009-08-30 15:00:00");
	TEST_APPROXIDATE("noon today", "2009-08-30 12:00:00");
	TEST_APPROXIDATE("noon yesterday", "2009-08-29 12:00:00");
	TEST_APPROXIDATE("January 5th noon pm", "2009-01-05 12:00:00");
	TEST_APPROXIDATE("10am noon", "2009-08-29 12:00:00");

	TEST_APPROXIDATE("last tuesday", "2009-08-25 19:20:00");
	TEST_APPROXIDATE("July 5th", "2009-07-05 19:20:00");
	TEST_APPROXIDATE("06/05/2009", "2009-06-05 19:20:00");
	TEST_APPROXIDATE("06.05.2009", "2009-05-06 19:20:00");

	TEST_APPROXIDATE("Jun 6, 5AM", "2009-06-06 05:00:00");
	TEST_APPROXIDATE("5AM Jun 6", "2009-06-06 05:00:00");
	TEST_APPROXIDATE("6AM, June 7, 2009", "2009-06-07 06:00:00");

	TEST_APPROXIDATE("2008-12-01", "2008-12-01 19:20:00");
	TEST_APPROXIDATE("2009-12-01", "2009-12-01 19:20:00");

	TEST_DATE_FORMAT_HUMAN(18000, "5 hours ago");
	TEST_DATE_FORMAT_HUMAN(432000, "Tue Aug 25 19:20");
	TEST_DATE_FORMAT_HUMAN(1728000, "Mon Aug 10 19:20");
	TEST_DATE_FORMAT_HUMAN(13000000, "Thu Apr 2 08:13");
	TEST_DATE_FORMAT_HUMAN(31449600, "Aug 31 2008");
	TEST_DATE_FORMAT_HUMAN(37500000, "Jun 22 2008");
	TEST_DATE_FORMAT_HUMAN(55188000, "Dec 1 2007");
	TEST_DATE_FORMAT_HUMAN(630000000, "Sep 13 1989");

	return test_done();
}
