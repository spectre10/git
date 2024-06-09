#include "test-lib.h"
#include "lib-oid.h"
#include "oid-array.h"
#include "strvec.h"
#include "hex.h"

#define TEST_MIN(x, y) (((x) <= (y)) ? (x) : (y))

#define INPUT "88", "44", "aa", "55"
#define INPUT_DUP INPUT, INPUT, INPUT
#define INPUT_ONLY_DUP "55", "55"
#define INPUT_ALMOST_DUP "55", "550000000000000000000000000000000000001"
#define ENUMERATION_RESULT_SORTED "44", "55", "88", "aa"

LAST_ARG_MUST_BE_NULL
static int fill_array(struct oid_array *array, ...)
{
	const char *arg;
	struct strvec hexes = STRVEC_INIT;
	va_list params;

	va_start(params, array);
	while ((arg = va_arg(params, const char *)))
		strvec_push(&hexes, arg);
	va_end(params);

	for (size_t i = 0; i < hexes.nr; i++) {
		struct object_id oid;

		if (!check_int(get_oid_arbitrary_hex(hexes.v[i], &oid), ==, 0))
			return -1;
		oid_array_append(array, &oid);
	}

	strvec_clear(&hexes);
	return 0;
}

static int add_to_oid_array(const struct object_id *oid, void *data)
{
	struct oid_array *array = data;
	oid_array_append(array, oid);
	return 0;
}

#define TEST_ENUMERATION(I, DESC) \
	do { \
		int skip = test__run_begin(); \
		if (!skip) { \
			struct oid_array input = OID_ARRAY_INIT, \
					 expect = OID_ARRAY_INIT, \
					 actual = OID_ARRAY_INIT; \
			if (fill_array(&input, I, NULL)) \
				break; \
			if (fill_array(&expect, ENUMERATION_RESULT_SORTED, NULL)) \
				break; \
			oid_array_for_each_unique(&input, add_to_oid_array, &actual); \
			check_int(actual.nr, ==, expect.nr); \
 \
			for (int i = 0; i < TEST_MIN(actual.nr, expect.nr); i++) \
				if (!check(oideq(&actual.oid[i], \
						 &expect.oid[i]))) { \
					test_msg( \
						"expected: %s\n       got: %s\n     index: %d", \
						oid_to_hex(&expect.oid[i]), \
						oid_to_hex(&actual.oid[i]), \
						i); \
				} \
			oid_array_clear(&actual); \
			oid_array_clear(&input); \
			oid_array_clear(&expect); \
		} \
		test__run_end(!skip, TEST_LOCATION(), DESC); \
	} while (0)

#define TEST_LOOKUP(I, QUERY, CONDITION, DESC) \
	do { \
		int skip = test__run_begin(); \
		if (!skip) { \
			struct object_id oid_query; \
			struct oid_array array = OID_ARRAY_INIT; \
			int ret = 0; \
 \
			if (!check_int(get_oid_arbitrary_hex(QUERY, &oid_query), \
				       ==, 0)) \
				break; \
			if (fill_array(&array, I, NULL)) \
				break; \
			ret = oid_array_lookup(&array, &oid_query); \
			if (!check(CONDITION)) \
				test_msg("oid query for lookup: %s", \
					 hash_to_hex(oid_query.hash)); \
			oid_array_clear(&array); \
		} \
		test__run_end(!skip, TEST_LOCATION(), DESC); \
	} while (0)

static void setup(void)
{
	int algo = init_hash_algo();
	if (check(algo != GIT_HASH_UNKNOWN)) {
		/* this is needed because oid_array_lookup() uses the_hash_algo internally */
		the_hash_algo = &hash_algos[algo];
	}
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	if (!TEST(setup(), "setup"))
		test_skip_all("hash algo initialization failed");

	TEST_ENUMERATION(INPUT, "ordered enumeration works");
	TEST_ENUMERATION(INPUT_DUP,
			 "ordered enumeration with duplicate suppression works");

	/* ret is the return value of oid_array_lookup() */
	TEST_LOOKUP(INPUT, "55", ret == 1, "lookup works");
	TEST_LOOKUP(INPUT, "33", ret < 1, "lookup non-existent entry");
	TEST_LOOKUP(INPUT_DUP, "66", ret < 0, "lookup with duplicates works");
	TEST_LOOKUP(INPUT_DUP, "55", ret >= 3 && ret <= 5,
		    "lookup non-existent entry with duplicates");
	TEST_LOOKUP(INPUT_ALMOST_DUP, "55", ret == 0,
		    "lookup with almost duplicate values works");
	TEST_LOOKUP(INPUT_ONLY_DUP, "55", ret >= 0 && ret <= 1,
		    "lookup with single duplicate value works");

	return test_done();
}
