#include "test-lib.h"
#include "hex.h"
#include "oid-array.h"
#include "strbuf.h"

#define INPUT "88", "44", "aa", "55"
#define INPUT_DUP INPUT, INPUT, INPUT
#define INPUT_ONLY_DUP "55", "55"
#define ENUMERATION_RESULT_SORTED "44", "55", "88", "aa"

/* callback function for for_each used for printing */
static int print_cb(const struct object_id *oid, void *data)
{
	int *i = data;
	test_msg("%d. %s", *i, oid_to_hex(oid));
	*i += 1;
	return 0;
}

/* prints the oid_array with a message title */
static void print_oid_array(struct oid_array *array, char *msg)
{
	int i = 0;
	test_msg("%s", msg);
	oid_array_for_each(array, print_cb, &i);
}

/* fills the hex strbuf with alternating characters from 'c' */
static void fill_hex_strbuf(struct strbuf *hex, char *c)
{
	static int sz = -1;
	size_t i;

	if (sz == -1) {
		char *algo_env = getenv("GIT_TEST_DEFAULT_HASH");
		if (algo_env && !strcmp(algo_env, "sha256"))
			sz = GIT_SHA256_HEXSZ;
		else
			sz = GIT_SHA1_HEXSZ;
	}

	for (i = 0; i < sz; i++)
		strbuf_addch(hex, (i & 1) ? c[1] : c[0]);
}

/* populates object_id with hexadecimal representation generated from 'c' */
static int get_oid_hex_input(struct object_id *oid, char *c)
{
	int ret;
	struct strbuf hex = STRBUF_INIT;

	fill_hex_strbuf(&hex, c);
	ret = get_oid_hex_any(hex.buf, oid);
	if (ret == GIT_HASH_UNKNOWN)
		test_msg("not a valid hexadecimal oid: %s", hex.buf);
	strbuf_release(&hex);
	return ret;
}

/* populates the oid_array with input from entries array */
static int populate_oid_array(struct oid_array *oidarray, char **entries,
			      size_t len)
{
	size_t i;
	struct object_id oid;

	for (i = 0; i < len; i++) {
		if (!check_int(get_oid_hex_input(&oid, entries[i]), !=,
			       GIT_HASH_UNKNOWN))
			return -1;
		oid_array_append(oidarray, &oid);
	}
	return 0;
}

/* callback function for enumeration test */
static int add_to_oid_array(const struct object_id *oid, void *data)
{
	struct oid_array *array = data;
	oid_array_append(array, oid);
	return 0;
}

static void test_enumeration(char *input_entries[], size_t input_size,
			     char *expected_entries[], size_t expected_size)
{
	int i;
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

	for (i = 0; i < expected_size; i++) {
		if (!check_int(oideq(&actual.oid[i], &expect.oid[i]), ==, 1)) {
			test_msg("failed at index %d", i);
			print_oid_array(&expect, "expected array content:");
			print_oid_array(&actual, "actual array content:");
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

static void lookup_setup(void (*f)(struct strbuf *buf, struct oid_array *array))
{
	struct oid_array array = OID_ARRAY_INIT;
	struct strbuf buf = STRBUF_INIT;

	f(&buf, &array);
	oid_array_clear(&array);
	strbuf_release(&buf);
}

#define LOOKUP_INPUT(INPUT, QUERY, NAME, CONDITION)                           \
	static void t_##NAME(struct strbuf *buf UNUSED,                       \
			     struct oid_array *array)                         \
	{                                                                     \
		struct object_id oid_query;                                   \
		char *input_entries[] = { INPUT };                            \
		size_t input_size = ARRAY_SIZE(input_entries);                \
		int ret;                                                      \
		if (!check_int(get_oid_hex_input(&oid_query, QUERY), !=,      \
			       GIT_HASH_UNKNOWN))                             \
			return;                                               \
		if (!check_int(populate_oid_array(array, input_entries,       \
						  input_size),                \
			       !=, -1))                                       \
			return;                                               \
		ret = oid_array_lookup(array, &oid_query);                    \
		if (!check(CONDITION)) {                                      \
			print_oid_array(array, "array content:");             \
			test_msg("oid query for lookup: %s", oid_query.hash); \
		}                                                             \
	}

/* ret is return value of oid_array_lookup() */
LOOKUP_INPUT(INPUT, "55", lookup, ret == 1)
LOOKUP_INPUT(INPUT, "33", lookup_nonexist, ret < 1)
LOOKUP_INPUT(INPUT_DUP, "66", lookup_nonexist_dup, ret < 0)
LOOKUP_INPUT(INPUT_DUP, "55", lookup_dup, ret >= 3 && ret <= 5)
LOOKUP_INPUT(INPUT_ONLY_DUP, "55", lookup_only_dup, ret >= 0 && ret <= 1)

static void t_lookup_almost_dup(struct strbuf *hex, struct oid_array *array)
{
	struct object_id oid;

	fill_hex_strbuf(hex, "55");
	if (!check_int(get_oid_hex_any(hex->buf, &oid), !=, GIT_HASH_UNKNOWN))
		return;

	oid_array_append(array, &oid);
	/* last character different */
	hex->buf[hex->len - 1] = 'f';
	if (!check_int(get_oid_hex_any(hex->buf, &oid), !=, GIT_HASH_UNKNOWN))
		return;

	oid_array_append(array, &oid);
	if (!check_int(oid_array_lookup(array, &oid), ==, 1)) {
		print_oid_array(array, "array content:");
		test_msg("oid query for lookup: %s", hex->buf);
	}
}

int cmd_main(int argc, const char **argv)
{
	TEST(t_ordered_enumeration_non_duplicate(),
	     "ordered enumeration works");
	TEST(t_ordered_enumeration_duplicate(),
	     "ordered enumeration with duplicate suppression works");
	TEST(lookup_setup(t_lookup), "lookup works");
	TEST(lookup_setup(t_lookup_nonexist), "lookup non-existent entry");
	TEST(lookup_setup(t_lookup_dup), "lookup with duplicates works");
	TEST(lookup_setup(t_lookup_nonexist_dup),
	     "lookup non-existent entry with duplicates");
	TEST(lookup_setup(t_lookup_almost_dup),
	     "lookup with almost duplicate values works");
	TEST(lookup_setup(t_lookup_only_dup),
	     "lookup with single duplicate value works");

	return test_done();
}
