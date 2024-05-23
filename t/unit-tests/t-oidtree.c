#include "test-lib.h"
#include "test-lib-functions.h"
#include "oidtree.h"

#define FILL_G(tree, ...)                          \
	{                                          \
		char *y[] = { __VA_ARGS__ };       \
		fill_tree(tree, y, ARRAY_SIZE(y)); \
	}

static int fill_tree(struct oidtree *ot, char *s[], int n)
{
	int i;
	struct object_id oid;

	for (i = 0; i < n; i++) {
		get_oid_arbitrary(s[i], GIT_HASH_SHA1, &oid);
		oidtree_insert(ot, &oid);
	}
	return 0;
}

#define check_contains(x, y)                                   \
	{                                                      \
		struct object_id oid;                          \
		get_oid_arbitrary(x, GIT_HASH_SHA1, &oid);     \
		check_int(oidtree_contains(&ot, &oid), ==, y); \
	}

static void test_1(void)
{
	struct oidtree ot;
	oidtree_init(&ot);
	FILL_G(&ot, "444", "1", "2", "3", "4", "5", "a", "b", "c", "d", "e");
	check_contains("44", 1);
	check_contains("441", 0);
	check_contains("440", 0);
	check_contains("444", 1);
	check_contains("4440", 0);
	check_contains("4444", 1);
	check(1 == 1);
	oidtree_clear(&ot);
}

int cmd_main(int argc, const char **argv)
{
	TEST(test_1(), "test");
	return test_done();
}
