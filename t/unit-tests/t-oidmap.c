#include "test-lib.h"
#include "lib-oid.h"
#include "oidmap.h"
#include "hash.h"
#include "hex.h"

/*
 * elements we will put in oidmap structs are made of a key: the entry.oid
 * field, which is of type struct object_id, and a value: the name field (could
 * be a refname for example)
 */
struct test_entry {
	struct oidmap_entry entry;
	char name[FLEX_ARRAY];
};

static const char *key_val[][2] = { { "11", "one" },
				    { "22", "two" },
				    { "33", "three" } };

static int put_and_check_null(struct oidmap *map, const char *hex,
			      const char *entry_name)
{
	struct test_entry *entry;

	FLEX_ALLOC_STR(entry, name, entry_name);
	if (get_oid_arbitrary_hex(hex, &entry->entry.oid))
		return -1;
	check(oidmap_put(map, entry) == NULL);
	return 0;
}

static void setup(void (*f)(struct oidmap *map))
{
	struct oidmap map = OIDMAP_INIT;
	int ret = 0;

	for (size_t i = 0; i < ARRAY_SIZE(key_val); i++)
		if ((ret = put_and_check_null(&map, key_val[i][0],
					      key_val[i][1])))
			break;

	if (!ret)
		f(&map);
	oidmap_free(&map, 1);
}

static void t_replace(struct oidmap *map)
{
	struct test_entry *entry, *prev;

	FLEX_ALLOC_STR(entry, name, "un");
	if (get_oid_arbitrary_hex("11", &entry->entry.oid))
		return;
	prev = oidmap_put(map, entry);
	if (!check(prev != NULL))
		return;
	check_str(prev->name, "one");
	free(prev);

	FLEX_ALLOC_STR(entry, name, "deux");
	if (get_oid_arbitrary_hex("22", &entry->entry.oid))
		return;
	prev = oidmap_put(map, entry);
	if (!check(prev != NULL))
		return;
	check_str(prev->name, "two");
	free(prev);
}

static void t_get(struct oidmap *map)
{
	struct test_entry *entry;
	struct object_id oid;

	if (get_oid_arbitrary_hex("22", &oid))
		return;
	entry = oidmap_get(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "two");

	if (get_oid_arbitrary_hex("44", &oid))
		return;
	check(oidmap_get(map, &oid) == NULL);

	if (get_oid_arbitrary_hex("11", &oid))
		return;
	entry = oidmap_get(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "one");
}

static void t_remove(struct oidmap *map)
{
	struct test_entry *entry;
	struct object_id oid;

	if (get_oid_arbitrary_hex("11", &oid))
		return;
	entry = oidmap_remove(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "one");
	check(oidmap_get(map, &oid) == NULL);
	free(entry);

	if (get_oid_arbitrary_hex("22", &oid))
		return;
	entry = oidmap_remove(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "two");
	check(oidmap_get(map, &oid) == NULL);
	free(entry);

	if (get_oid_arbitrary_hex("44", &oid))
		return;
	check(oidmap_remove(map, &oid) == NULL);
}

static int key_val_contains(struct test_entry *entry)
{
	/* the test is small enough to be able to bear O(n) */
	for (size_t i = 0; i < ARRAY_SIZE(key_val); i++) {
		if (!strcmp(key_val[i][1], entry->name)) {
			struct object_id oid;
			if (get_oid_arbitrary_hex(key_val[i][0], &oid))
				return -1;
			if (oideq(&entry->entry.oid, &oid))
				return 0;
		}
	}
	return 1;
}

static void t_iterate(struct oidmap *map)
{
	struct oidmap_iter iter;
	struct test_entry *entry;
	int ret;

	oidmap_iter_init(map, &iter);
	while ((entry = oidmap_iter_next(&iter))) {
		if (!check_int((ret = key_val_contains(entry)), ==, 0)) {
			if (ret == -1)
				return;
			test_msg("obtained entry was not given in the input\n"
	    			 "  name: %s\n   oid: %s\n",
				 entry->name, oid_to_hex(&entry->entry.oid));
		}
	}
	check_int(hashmap_get_size(&map->map), ==, ARRAY_SIZE(key_val));
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	TEST(setup(t_replace), "replace works");
	TEST(setup(t_get), "get works");
	TEST(setup(t_remove), "remove works");
	TEST(setup(t_iterate), "iterate works");
	return test_done();
}
