#include "test-lib.h"
#include "lib-oid.h"
#include "oidtree.h"
#include "hash.h"

#define FILL_TREE(tree, ...)                           \
	{                                              \
		char *y[] = { __VA_ARGS__ };           \
		fill_tree_loc(tree, y, ARRAY_SIZE(y)); \
	}
static int fill_tree_loc(struct oidtree *ot, char *s[], int n)
{
	int i;
	struct object_id oid;
	for (i = 0; i < n; i++) {
		get_oid_arbitrary_hex(s[i], &oid);
		oidtree_insert(ot, &oid);
	}
	return 0;
}

static void check_contains(struct oidtree *ot, const char *hex, int expected)
{
	struct object_id oid;
	get_oid_arbitrary_hex(hex, &oid);
	check_int(oidtree_contains(ot, &oid), ==, expected);
}

static enum cb_next check_oid_cb(const struct object_id *oid, void *data)
{
	const char *hex = data;
	struct object_id expected;
	get_oid_arbitrary_hex(hex, &expected);
	check(oideq(oid, &expected));
	return CB_CONTINUE;
}

static void check_each(struct oidtree *ot, char *hex)
{
	struct object_id oid;
	get_oid_arbitrary_hex(hex, &oid);
	oidtree_each(ot, &oid, 40, check_oid_cb, hex);
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
	check_each(ot, "12300");
	check_each(ot, "3211");
	check_each(ot, "3210");
	check_each(ot, "32100");
}

int cmd_main(int argc, const char **argv)
{
	TEST(setup(t_insert), "oidtree insert and contains");
	TEST(setup(t_each), "oidtree each");
	return test_done();
}
