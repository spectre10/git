#define USE_THE_REPOSITORY_VARIABLE

#include "test-lib.h"
#include "lib-oid.h"
#include "oid-array.h"
#include "hex.h"

static inline int test_min(int a, int b)
{
	return a <= b ? a : b;
}

static int fill_array(struct oid_array *array, const char *hexes[], size_t n)
{
	for (size_t i = 0; i < n; i++) {
		struct object_id oid;

		if (!check_int(get_oid_arbitrary_hex(hexes[i], &oid), ==, 0))
			return -1;
		oid_array_append(array, &oid);
	}
	return 0;
}

static int add_to_oid_array(const struct object_id *oid, void *data)
{
	struct oid_array *array = data;
	oid_array_append(array, oid);
	return 0;
}

static void t_enumeration(const char *input_args[], size_t input_sz,
			  const char *result[], size_t result_sz)
{
	struct oid_array input = OID_ARRAY_INIT, expect = OID_ARRAY_INIT,
			 actual = OID_ARRAY_INIT;

	if (fill_array(&input, input_args, input_sz))
		return;
	if (fill_array(&expect, result, result_sz))
		return;

	oid_array_for_each_unique(&input, add_to_oid_array, &actual);
	check_int(actual.nr, ==, expect.nr);

	for (size_t i = 0; i < test_min(actual.nr, expect.nr); i++)
		if (!check(oideq(&actual.oid[i], &expect.oid[i])))
			test_msg("expected: %s\n       got: %s\n     index: %" PRIuMAX,
				 oid_to_hex(&expect.oid[i]), oid_to_hex(&actual.oid[i]),
				 (uintmax_t)i);
	oid_array_clear(&actual);
	oid_array_clear(&input);
	oid_array_clear(&expect);
}

#define TEST_ENUMERATION(input, result, desc)                                     \
	TEST(t_enumeration(input, ARRAY_SIZE(input), result, ARRAY_SIZE(result)), \
			   desc " works")

static int t_lookup(struct object_id *oid_query, const char *query,
		    const char *hexes[], size_t n)
{
	struct oid_array array = OID_ARRAY_INIT;
	int ret;

	if (!check_int(get_oid_arbitrary_hex(query, oid_query), ==, 0))
		return INT_MIN;
	if (fill_array(&array, hexes, n))
		return INT_MIN;
	ret = oid_array_lookup(&array, oid_query);

	oid_array_clear(&array);
	return ret;
}

#define TEST_LOOKUP(input_args, query, condition, desc)                   \
	do {                                                              \
		int skip = test__run_begin();                             \
		if (!skip) {                                              \
			struct object_id oid_query;                       \
			int ret = t_lookup(&oid_query, query, input_args, \
					   ARRAY_SIZE(input_args));       \
                                                                          \
			if (ret != INT_MIN && !check(condition))          \
				test_msg("oid query for lookup: %s",      \
					 oid_to_hex(&oid_query));         \
		}                                                         \
		test__run_end(!skip, TEST_LOCATION(), desc " works");     \
	} while (0)

static void setup(void)
{
	int algo = init_hash_algo();
	/* because the_hash_algo is used by oid_array_lookup() internally */
	if (check_int(algo, !=, GIT_HASH_UNKNOWN))
		repo_set_hash_algo(the_repository, algo);
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	const char *arr_input[] = { "88", "44", "aa", "55" };
	const char *arr_input_dup[] = { "88", "44", "aa", "55",
					"88", "44", "aa", "55",
					"88", "44", "aa", "55" };
	const char *res_sorted[] = { "44", "55", "88", "aa" };

	if (!TEST(setup(), "setup"))
		test_skip_all("hash algo initialization failed");

	TEST_ENUMERATION(arr_input, res_sorted, "ordered enumeration");
	TEST_ENUMERATION(arr_input_dup, res_sorted,
			 "ordered enumeration with duplicate suppression");

	/* ret is the return value of oid_array_lookup() */
	TEST_LOOKUP(arr_input, "55", ret == 1, "lookup");
	TEST_LOOKUP(arr_input, "33", ret < 0, "lookup non-existent entry");
	TEST_LOOKUP(arr_input_dup, "55", ret >= 3 && ret <= 5,
		    "lookup with duplicates");
	TEST_LOOKUP(arr_input_dup, "66", ret < 0,
		    "lookup non-existent entry with duplicates");
	TEST_LOOKUP(
		((const char *[]){"55", init_hash_algo() == GIT_HASH_SHA1 ?
					"5500000000000000000000000000000000000001" :
					"5500000000000000000000000000000000000000000000000000000000000001" }),
		"55", ret == 0, "lookup with almost duplicate values");
	TEST_LOOKUP(((const char *[]){ "55", "55" }), "55",
		    ret >= 0 && ret <= 1, "lookup with single duplicate value");

	return test_done();
}
