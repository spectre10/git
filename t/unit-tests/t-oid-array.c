#define USE_THE_REPOSITORY_VARIABLE

#include "test-lib.h"
#include "lib-oid.h"
#include "oid-array.h"
#include "hex.h"

static inline size_t test_min(size_t a, size_t b)
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
	if (!check_uint(array->nr, ==, n))
		return -1;
	return 0;
}

static int add_to_oid_array(const struct object_id *oid, void *data)
{
	struct oid_array *array = data;

	oid_array_append(array, oid);
	return 0;
}

static void t_enumeration(const char **input_args, size_t input_sz,
			  const char **result, size_t result_sz)
{
	struct oid_array input = OID_ARRAY_INIT, expect = OID_ARRAY_INIT,
			 actual = OID_ARRAY_INIT;
	size_t i;

	if (fill_array(&input, input_args, input_sz))
		return;
	if (fill_array(&expect, result, result_sz))
		return;

	oid_array_for_each_unique(&input, add_to_oid_array, &actual);
	check_uint(actual.nr, ==, expect.nr);

	for (i = 0; i < test_min(actual.nr, expect.nr); i++) {
		if (!check(oideq(&actual.oid[i], &expect.oid[i])))
			test_msg("expected: %s\n       got: %s\n     index: %" PRIuMAX,
				 oid_to_hex(&expect.oid[i]), oid_to_hex(&actual.oid[i]),
				 (uintmax_t)i);
	}
	check_uint(i, ==, result_sz);

	oid_array_clear(&actual);
	oid_array_clear(&input);
	oid_array_clear(&expect);
}

#define TEST_ENUMERATION(input, result, desc)                                     \
	TEST(t_enumeration(input, ARRAY_SIZE(input), result, ARRAY_SIZE(result)), \
			   desc " works")

static void t_lookup(const char **input_hexes, size_t n, const char *query_hex,
		     int lower_bound, int upper_bound)
{
	struct oid_array array = OID_ARRAY_INIT;
	struct object_id oid_query;
	int ret;

	if (get_oid_arbitrary_hex(query_hex, &oid_query))
		return;
	if (fill_array(&array, input_hexes, n))
		return;
	ret = oid_array_lookup(&array, &oid_query);

	if (!check_int(ret, <=, upper_bound) ||
	    !check_int(ret, >=, lower_bound))
		test_msg("oid query for lookup: %s", oid_to_hex(&oid_query));

	oid_array_clear(&array);
}

#define TEST_LOOKUP(input_hexes, query, lower_bound, upper_bound, desc) \
	TEST(t_lookup(input_hexes, ARRAY_SIZE(input_hexes), query,      \
		      lower_bound, upper_bound),                        \
	     desc " works")

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
	const char *nearly_55;

	if (!TEST(setup(), "setup"))
		test_skip_all("hash algo initialization failed");

	TEST_ENUMERATION(arr_input, res_sorted, "ordered enumeration");
	TEST_ENUMERATION(arr_input_dup, res_sorted,
			 "ordered enumeration with duplicate suppression");

	/* ret is the return value of oid_array_lookup() */
	TEST_LOOKUP(arr_input, "55", 1, 1, "lookup");
	TEST_LOOKUP(arr_input, "33", INT_MIN, -1, "lookup non-existent entry");
	TEST_LOOKUP(arr_input_dup, "55", 3, 5, "lookup with duplicates");
	TEST_LOOKUP(arr_input_dup, "66", INT_MIN, -1,
		    "lookup non-existent entry with duplicates");

	nearly_55 = init_hash_algo() == GIT_HASH_SHA1 ?
			"5500000000000000000000000000000000000001" :
			"5500000000000000000000000000000000000000000000000000000000000001";
	TEST_LOOKUP(((const char *[]){ "55", nearly_55 }), "55", 0, 0,
		    "lookup with almost duplicate values");
	TEST_LOOKUP(((const char *[]){ "55", "55" }), "55", 0, 1,
		    "lookup with single duplicate value");

	return test_done();
}
