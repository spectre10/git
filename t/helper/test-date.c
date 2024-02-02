#include "test-tool.h"
#include "date.h"
#include "trace.h"

static const char *usage_msg = "\n"
"  test-tool date timestamp [date]...\n"
"  test-tool date getnanos [start-nanos]\n"
"  test-tool date is64bit\n"
"  test-tool date time_t-is64bit\n";

static void parse_approx_timestamp(const char **argv)
{
	for (; *argv; argv++) {
		timestamp_t t;
		t = approxidate(*argv);
		printf("%s -> %"PRItime"\n", *argv, t);
	}
}

static void getnanos(const char **argv)
{
	double seconds = getnanotime() / 1.0e9;

	if (*argv)
		seconds -= strtod(*argv, NULL);
	printf("%lf\n", seconds);
}

int cmd__date(int argc UNUSED, const char **argv)
{

	argv++;
	if (!*argv)
		usage(usage_msg);
	if (!strcmp(*argv, "timestamp"))
		parse_approx_timestamp(argv+1);
	else if (!strcmp(*argv, "getnanos"))
		getnanos(argv+1);
	else if (!strcmp(*argv, "is64bit"))
		return !check_prereq_TIME_IS_64BIT();
	else if (!strcmp(*argv, "time_t-is64bit"))
		return !check_prereq_TIME_T_IS_64BIT();
	else
		usage(usage_msg);
	return 0;
}
