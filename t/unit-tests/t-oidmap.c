#include "test-lib.h"
#include "lib-oid.h"
#include "oidmap.h"
#include "hash.h"
#include "hex.h"

/* key is an oid and value is a name (could be a refname for example) */
struct test_entry {
	struct oidmap_entry entry;
	char name[FLEX_ARRAY];
};

static void setup(void (*f)(struct oidmap *map))
{
	struct oidmap map = OIDMAP_INIT;
	f(&map);
	oidmap_free(&map, 1);
}

static void put_and_check_null(struct oidmap *map, const char *hex,
			       const char *entry_name)
{
	struct test_entry *entry;

	FLEX_ALLOC_STR(entry, name, entry_name);
	get_oid_arbitrary_hex(hex, &entry->entry.oid);
	check(oidmap_put(map, entry) == NULL);
}

static void t_put(struct oidmap *map)
{
	put_and_check_null(map, "11", "one");
	put_and_check_null(map, "22", "two");
	put_and_check_null(map, "33", "three");
}

static void t_replace(struct oidmap *map)
{
	struct test_entry *entry, *prev;

	put_and_check_null(map, "11", "one");
	put_and_check_null(map, "22", "two");
	put_and_check_null(map, "33", "three");

	FLEX_ALLOC_STR(entry, name, "un");
	get_oid_arbitrary_hex("11", &entry->entry.oid);
	prev = oidmap_put(map, entry);
	check_str(prev->name, "one");
	free(prev);

	FLEX_ALLOC_STR(entry, name, "deux");
	get_oid_arbitrary_hex("22", &entry->entry.oid);
	prev = oidmap_put(map, entry);
	check_str(prev->name, "two");
	free(prev);
}

static void t_get(struct oidmap *map)
{
	struct test_entry *entry;
	struct object_id oid;

	put_and_check_null(map, "11", "one");
	put_and_check_null(map, "22", "two");
	put_and_check_null(map, "33", "three");

	get_oid_arbitrary_hex("22", &oid);
	entry = oidmap_get(map, &oid);
	check_str(entry->name, "two");

	get_oid_arbitrary_hex("44", &oid);
	check(oidmap_get(map, &oid) == NULL);

	get_oid_arbitrary_hex("11", &oid);
	entry = oidmap_get(map, &oid);
	check_str(entry->name, "one");
}

static void t_remove(struct oidmap *map)
{
	struct test_entry *entry;
	struct object_id oid;

	put_and_check_null(map, "11", "one");
	put_and_check_null(map, "22", "two");
	put_and_check_null(map, "33", "three");

	get_oid_arbitrary_hex("11", &oid);
	entry = oidmap_remove(map, &oid);
	check_str(entry->name, "one");
	check(oidmap_get(map, &oid) == NULL);
	free(entry);

	get_oid_arbitrary_hex("22", &oid);
	entry = oidmap_remove(map, &oid);
	check_str(entry->name, "two");
	check(oidmap_get(map, &oid) == NULL);
	free(entry);

	get_oid_arbitrary_hex("44", &oid);
	check(oidmap_remove(map, &oid) == NULL);
}

static int input_contains(const char *input[][2], size_t n,
			  struct test_entry *entry)
{
	/* the test is small enough to be able to bear O(n) */
	for (size_t i = 0; i < n; i++) {
		if (!strcmp(input[i][1], entry->name)) {
			struct object_id oid;
			get_oid_arbitrary_hex(input[i][0], &oid);
			if (oideq(&entry->entry.oid, &oid))
				return 0;
		}
	}
	return -1;
}

static void t_iterate(struct oidmap *map)
{
	struct oidmap_iter iter;
	struct test_entry *entry;
	/* input[i][1] is hex (IOW key) and input[i][2] is name (IOW value) */
	const char *input[][2] = { { "11", "one" },
				   { "22", "two" },
				   { "33", "three" } };

	for (size_t i = 0; i < ARRAY_SIZE(input); i++)
		put_and_check_null(map, input[i][0], input[i][1]);

	oidmap_iter_init(map, &iter);
	while ((entry = oidmap_iter_next(&iter))) {
		if (!check_int(input_contains(input, ARRAY_SIZE(input), entry),
			       ==, 0))
			test_msg("obtained entry was not given in the input\n  name: %s\n   oid: %s\n",
				 entry->name, oid_to_hex(&entry->entry.oid));
	}
	check_int(hashmap_get_size(&map->map), ==, ARRAY_SIZE(input));
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	TEST(setup(t_put), "put works");
	TEST(setup(t_replace), "replace works");
	TEST(setup(t_get), "get works");
	TEST(setup(t_remove), "remove works");
	TEST(setup(t_iterate), "iterate works");
	return test_done();
}
