#include "test-lib.h"
#include "date.h"
#include "strbuf.h"

static const char test_time[] = "1466000000 +0200";
enum prerequisites {
	TIME_IS_64BIT = 1 << 0,
	TIME_T_IS_64BIT = 1 << 1,
};

static void set_TZ_env(const char *zone)
{
	setenv("TZ", zone, 1);
	tzset();
}

#define CHECK_PREREQ(var, prereq)                                   \
	do {                                                        \
		if ((var) & prereq && !check_prereq_##prereq()) {   \
			test_skip("missing prerequisite " #prereq); \
			return 0;                                   \
		}                                                   \
	} while (0)

#define TEST_SHOW_DATE(format, time, expected, prereqs, zone)        \
	TEST(check_show_date(format, time, expected, prereqs, zone), \
	     "show date (%s) works", #format)

static int check_prereqs(unsigned int prereqs)
{
	CHECK_PREREQ(prereqs, TIME_IS_64BIT);
	CHECK_PREREQ(prereqs, TIME_T_IS_64BIT);

	return 1;
}

static void check_show_date(const char *format, const char *TIME,
			    const char *expected, unsigned int prereqs,
			    const char *zone)
{
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

	if (!check_str(show_date(t, tz, &mode), expected)) {
		const char *tz_env = getenv("TZ");
		const char *date = getenv("GIT_TEST_DATE_NOW");
		const char *lang = getenv("LANG");
		if (tz)
			test_msg("TZ: %s", tz_env);
		if (date)
			test_msg("GIT_TEST_DATE_NOW: %s", date);
		if (lang)
			test_msg("LANG: %s", lang);
	}

	if (strcmp(zone, ""))
		set_TZ_env("UTC");
	date_mode_release(&mode);
}

static void check_parse_date(const char *given, const char *expected,
			     const char *zone)
{
	struct strbuf result = STRBUF_INIT;
	timestamp_t t;
	int tz;

	if (strcmp(zone, ""))
		set_TZ_env(zone);

	parse_date(given, &result);
	if (sscanf(result.buf, "%" PRItime " %d", &t, &tz) == 2) {
		check_str(show_date(t, tz, DATE_MODE(ISO8601)), expected);
	} else {
		check_str("bad", expected);
	}

	if (strcmp(zone, ""))
		set_TZ_env("UTC");
	strbuf_release(&result);
}

#define TEST_PARSE_DATE(given, expected_output, zone)        \
	TEST(check_parse_date(given, expected_output, zone), \
	     "parse date (%s) works", #expected_output)

int cmd_main(int argc, const char **argv)
{
	// set_TZ_env("EST5");
	setenv("GIT_TEST_DATE_NOW", "1251660000", 1);
	setenv("LANG", "C", 1);

	TEST_SHOW_DATE("format-local:%Y-%m-%d %H:%M:%S", test_time,
		       "2016-06-15 09:13:20", 0, "");
	TEST_PARSE_DATE("2008-02-14 20:30:45", "2008-02-14 20:30:45 -0500", "");
	return test_done();
}
