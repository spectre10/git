#include "test-lib.h"
#include "lib-oid.h"
#include "oidtree.h"
#include "hash.h"
#include "hex.h"

#define FILL_TREE(tree, ...)                                   \
	{                                                      \
		const char *hexes[] = { __VA_ARGS__ };         \
		fill_tree_loc(tree, hexes, ARRAY_SIZE(hexes)); \
	}
static int fill_tree_loc(struct oidtree *ot, const char *hexes[], int n)
{
	for (int i = 0; i < n; i++) {
		struct object_id oid;
		get_oid_arbitrary_hex(hexes[i], &oid);
		oidtree_insert(ot, &oid);
	}
	return 0;
}

static void check_contains(struct oidtree *ot, const char *hex, int expected)
{
	struct object_id oid;

	get_oid_arbitrary_hex(hex, &oid);
	if (!check_int(oidtree_contains(ot, &oid), ==, expected))
		test_msg("oid: %s", oid_to_hex(&oid));
}

static enum cb_next check_each_cb(const struct object_id *oid, void *data)
{
	const char *hex = data;
	struct object_id expected;

	get_oid_arbitrary_hex(hex, &expected);
	if (!check(oideq(oid, &expected)))
		test_msg("expected: %s\n       got: %s",
			 hash_to_hex(expected.hash), hash_to_hex(oid->hash));
	return CB_CONTINUE;
}

static void check_each(struct oidtree *ot, char *hex, char *expected)
{
	struct object_id oid;
	get_oid_arbitrary_hex(hex, &oid);
	oidtree_each(ot, &oid, 40, check_each_cb, expected);
}

static void setup(void (*f)(struct oidtree *ot))
{
	struct oidtree ot;
	oidtree_init(&ot);
	f(&ot);
	oidtree_clear(&ot);
}

static void t_insert(struct oidtree *ot)
{
	FILL_TREE(ot, "444", "1", "2", "3", "4", "5", "a", "b", "c", "d", "e");
	check_contains(ot, "44", 0);
	check_contains(ot, "441", 0);
	check_contains(ot, "440", 0);
	check_contains(ot, "444", 1);
	check_contains(ot, "4440", 1);
	check_contains(ot, "4444", 0);
}

static void t_each(struct oidtree *ot)
{
	FILL_TREE(ot, "f", "9", "8", "123", "321", "a", "b", "c", "d", "e");
	check_each(ot, "12300", "123");
	check_each(ot, "3211", ""); /* should not reach callback */
	check_each(ot, "3210", "321");
	check_each(ot, "32100", "321");
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	TEST(setup(t_insert), "oidtree insert and contains");
	TEST(setup(t_each), "oidtree each");
	return test_done();
}
