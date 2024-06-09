#include "test-lib.h"
#include "lib-oid.h"
#include "oid-array.h"
#include "hex.h"

static inline int test_min(int a, int b)
{
	return a <= b ? a : b;
}

#define INPUT "88", "44", "aa", "55"
#define INPUT_DUP INPUT, INPUT, INPUT
#define INPUT_ONLY_DUP "55", "55"
#define INPUT_ALMOST_DUP "55", "550000000000000000000000000000000000001"
#define ENUMERATION_RESULT_SORTED "44", "55", "88", "aa"

LAST_ARG_MUST_BE_NULL
static int fill_array(struct oid_array *array, ...)
{
	const char *arg;
	va_list params;

	va_start(params, array);
	while ((arg = va_arg(params, const char *))) {
		struct object_id oid;

		if (!check_int(get_oid_arbitrary_hex(arg, &oid), ==, 0))
			return -1;
		oid_array_append(array, &oid);
	}
	va_end(params);

	return 0;
}

static int add_to_oid_array(const struct object_id *oid, void *data)
{
	struct oid_array *array = data;
	oid_array_append(array, oid);
	return 0;
}

#define TEST_ENUMERATION(input_args, desc) do { \
	int skip = test__run_begin(); \
	if (!skip) { \
		struct oid_array input = OID_ARRAY_INIT, \
				 expect = OID_ARRAY_INIT, \
				 actual = OID_ARRAY_INIT; \
\
		if (fill_array(&input, input_args, NULL)) \
			break; \
		if (fill_array(&expect, ENUMERATION_RESULT_SORTED, NULL)) \
			break; \
		oid_array_for_each_unique(&input, add_to_oid_array, &actual); \
		check_int(actual.nr, ==, expect.nr); \
\
		for (size_t i = 0; i < test_min(actual.nr, expect.nr); i++) \
			if (!check(oideq(&actual.oid[i], \
					 &expect.oid[i]))) { \
				test_msg( \
					"expected: %s\n       got: %s\n     index: %lu", \
					oid_to_hex(&expect.oid[i]), \
					oid_to_hex(&actual.oid[i]), \
					(unsigned long)i); \
			} \
		oid_array_clear(&actual); \
		oid_array_clear(&input); \
		oid_array_clear(&expect); \
	} \
	test__run_end(!skip, TEST_LOCATION(), desc " works"); \
} while (0)

#define TEST_LOOKUP(input_args, query, condition, desc) do { \
	int skip = test__run_begin(); \
	if (!skip) { \
		struct object_id oid_query; \
		struct oid_array array = OID_ARRAY_INIT; \
		int ret = 0; \
\
		if (!check_int(get_oid_arbitrary_hex(query, &oid_query), \
			       ==, 0)) \
			break; \
		if (fill_array(&array, input_args, NULL)) \
			break; \
		ret = oid_array_lookup(&array, &oid_query); \
		if (!check(condition)) \
			test_msg("oid query for lookup: %s", \
				 hash_to_hex(oid_query.hash)); \
		oid_array_clear(&array); \
	} \
	test__run_end(!skip, TEST_LOCATION(), desc " works"); \
} while (0)

static void setup(void)
{
	int algo = init_hash_algo();
	/* this is needed because oid_array_lookup() uses the_hash_algo internally */
	if (check(algo != GIT_HASH_UNKNOWN))
		the_hash_algo = &hash_algos[algo];
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	if (!TEST(setup(), "setup"))
		test_skip_all("hash algo initialization failed");

	TEST_ENUMERATION(INPUT, "ordered enumeration");
	TEST_ENUMERATION(INPUT_DUP,
			 "ordered enumeration with duplicate suppression");

	/* ret is the return value of oid_array_lookup() */
	TEST_LOOKUP(INPUT, "55", ret == 1, "lookup");
	TEST_LOOKUP(INPUT, "33", ret < 1, "lookup non-existent entry");
	TEST_LOOKUP(INPUT_DUP, "66", ret < 0, "lookup with duplicates");
	TEST_LOOKUP(INPUT_DUP, "55", ret >= 3 && ret <= 5,
		    "lookup non-existent entry with duplicates");
	TEST_LOOKUP(INPUT_ALMOST_DUP, "55", ret == 0,
		    "lookup with almost duplicate values");
	TEST_LOOKUP(INPUT_ONLY_DUP, "55", ret >= 0 && ret <= 1,
		    "lookup with single duplicate value");

	return test_done();
}
