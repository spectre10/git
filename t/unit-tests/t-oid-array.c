#include "test-lib.h"
#include "lib-oid.h"
#include "oid-array.h"
#include "strbuf.h"
#include "hex.h"

#define INPUT "88", "44", "aa", "55"
#define INPUT_DUP INPUT, INPUT, INPUT
#define INPUT_ONLY_DUP "55", "55"
#define INPUT_ALMOST_DUP "55", "550000000000000000000000000000000000001"
#define ENUMERATION_RESULT_SORTED "44", "55", "88", "aa"

static int populate_oid_array(struct oid_array *oidarray, char **entries,
			      size_t len)
{
	size_t i;
	struct object_id oid;

	for (i = 0; i < len; i++) {
		if (!check_int(get_oid_arbitrary_hex(entries[i], &oid), ==, 0))
			return -1;
		oid_array_append(oidarray, &oid);
	}
	return 0;
}

static int add_to_oid_array(const struct object_id *oid, void *data)
{
	struct oid_array *array = data;
	oid_array_append(array, oid);
	return 0;
}

static void test_enumeration(char *input_entries[], size_t input_size,
			     char *expected_entries[], size_t expected_size)
{
	struct oid_array input = OID_ARRAY_INIT;
	struct oid_array actual = OID_ARRAY_INIT;
	struct oid_array expect = OID_ARRAY_INIT;

	if (populate_oid_array(&input, input_entries, input_size) == -1)
		goto cleanup;

	if (populate_oid_array(&expect, expected_entries, expected_size) == -1)
		goto cleanup;

	oid_array_for_each_unique(&input, add_to_oid_array, &actual);
	if (!check_int(expect.nr, ==, expected_size) ||
	    !check_int(actual.nr, ==, expected_size))
		goto cleanup;

	for (int i = 0; i < expected_size; i++) {
		if (!check_int(oideq(&actual.oid[i], &expect.oid[i]), ==, 1)) {
			test_msg("failed at index %d", i);
			goto cleanup;
		}
	}

cleanup:
	oid_array_clear(&input);
	oid_array_clear(&actual);
	oid_array_clear(&expect);
}

#define ENUMERATION_INPUT(INPUT, RESULT, NAME)                                \
	static void t_ordered_enumeration_##NAME(void)                        \
	{                                                                     \
		char *input_entries[] = { INPUT };                            \
		char *expected_entries[] = { RESULT };                        \
		size_t input_size = ARRAY_SIZE(input_entries);                \
		size_t expected_size = ARRAY_SIZE(expected_entries);          \
		test_enumeration(input_entries, input_size, expected_entries, \
				 expected_size);                              \
	}

ENUMERATION_INPUT(INPUT, ENUMERATION_RESULT_SORTED, non_duplicate)
ENUMERATION_INPUT(INPUT_DUP, ENUMERATION_RESULT_SORTED, duplicate)

#define LOOKUP_INPUT(INPUT, QUERY, NAME, CONDITION)                          \
	static void t_##NAME(void)                                           \
	{                                                                    \
		struct object_id oid_query;                                  \
		struct oid_array array = OID_ARRAY_INIT;                     \
		char *input_entries[] = { INPUT };                           \
		size_t input_size = ARRAY_SIZE(input_entries);               \
		int ret;                                                     \
                                                                             \
		if (!check_int(get_oid_arbitrary_hex(QUERY, &oid_query), !=, \
			       -1))                                          \
			return;                                              \
		if (!check_int(populate_oid_array(&array, input_entries,     \
						  input_size), !=, -1))      \
			return;                                              \
		ret = oid_array_lookup(&array, &oid_query);                  \
		if (!check(CONDITION))                                       \
			test_msg("oid query for lookup: %s",                 \
				 hash_to_hex(oid_query.hash));               \
		oid_array_clear(&array);                                     \
	}

/* ret is return value of oid_array_lookup() */
LOOKUP_INPUT(INPUT, "55", lookup, ret == 1)
LOOKUP_INPUT(INPUT, "33", lookup_nonexist, ret < 1)
LOOKUP_INPUT(INPUT_DUP, "66", lookup_nonexist_dup, ret < 0)
LOOKUP_INPUT(INPUT_DUP, "55", lookup_dup, ret >= 3 && ret <= 5)
LOOKUP_INPUT(INPUT_ONLY_DUP, "55", lookup_only_dup, ret >= 0 && ret <= 1)
LOOKUP_INPUT(INPUT_ALMOST_DUP, "55", lookup_almost_dup, ret == 0)

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	TEST(t_ordered_enumeration_non_duplicate(),
	     "ordered enumeration works");
	TEST(t_ordered_enumeration_duplicate(),
	     "ordered enumeration with duplicate suppression works");
	TEST(t_lookup(), "lookup works");
	TEST(t_lookup_nonexist(), "lookup non-existent entry");
	TEST(t_lookup_dup(), "lookup with duplicates works");
	TEST(t_lookup_nonexist_dup(),
	     "lookup non-existent entry with duplicates");
	TEST(t_lookup_almost_dup(),
	     "lookup with almost duplicate values works");
	TEST(t_lookup_only_dup(), "lookup with single duplicate value works");
	return test_done();
}
