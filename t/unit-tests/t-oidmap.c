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

static void setup(void (*f)(struct oidmap *map))
{
	struct oidmap map = OIDMAP_INIT;
	int ret = 0;

	for (size_t i = 0; i < ARRAY_SIZE(key_val); i++){
		struct test_entry *entry;

		FLEX_ALLOC_STR(entry, name, key_val[i][1]);
		if (!check_int((ret = get_oid_arbitrary_hex(key_val[i][0], &entry->entry.oid)),
			       ==, 0)) {
			free(entry);
			break;
		}
		check(oidmap_put(&map, entry) == NULL);
	}

	if (!ret)
		f(&map);
	oidmap_free(&map, 1);
}

static void t_replace(struct oidmap *map)
{
	struct test_entry *entry, *prev;

	FLEX_ALLOC_STR(entry, name, "un");
	if (!check_int(get_oid_arbitrary_hex("11", &entry->entry.oid), ==, 0))
		return;
	prev = oidmap_put(map, entry);
	if (!check(prev != NULL))
		return;
	check_str(prev->name, "one");
	free(prev);

	FLEX_ALLOC_STR(entry, name, "deux");
	if (!check_int(get_oid_arbitrary_hex("22", &entry->entry.oid), ==, 0))
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

	if (!check_int(get_oid_arbitrary_hex("22", &oid), ==, 0))
		return;
	entry = oidmap_get(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "two");

	if (!check_int(get_oid_arbitrary_hex("44", &oid), ==, 0))
		return;
	check(oidmap_get(map, &oid) == NULL);

	if (!check_int(get_oid_arbitrary_hex("11", &oid), ==, 0))
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

	if (!check_int(get_oid_arbitrary_hex("11", &oid), ==, 0))
		return;
	entry = oidmap_remove(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "one");
	check(oidmap_get(map, &oid) == NULL);
	free(entry);

	if (!check_int(get_oid_arbitrary_hex("22", &oid), ==, 0))
		return;
	entry = oidmap_remove(map, &oid);
	if (!check(entry != NULL))
		return;
	check_str(entry->name, "two");
	check(oidmap_get(map, &oid) == NULL);
	free(entry);

	if (!check_int(get_oid_arbitrary_hex("44", &oid), ==, 0))
		return;
	check(oidmap_remove(map, &oid) == NULL);
}

static int key_val_contains(struct test_entry *entry)
{
	for (size_t i = 0; i < ARRAY_SIZE(key_val); i++) {
		struct object_id oid;

		if (check_int(get_oid_arbitrary_hex(key_val[i][0], &oid), ==, 0)) {
			if (oideq(&entry->entry.oid, &oid)) {
				if (!strcmp(key_val[i][1], "USED"))
					return 2;
				key_val[i][1] = "USED";
				return 0;
			}
		} else {
			return -1;
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
			if (ret == 2)
				test_msg("duplicate entry detected\n"
					 "  name: %s\n   oid: %s\n",
					 entry->name, oid_to_hex(&entry->entry.oid));
			else if (ret == 1)
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
