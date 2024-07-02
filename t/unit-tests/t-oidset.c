#include "test-lib.h"
#include "oidset.h"
#include "lib-oid.h"
#include "hex.h"

static const char *const hexes[] = { "00", "11", "22", "33", "aa", "cc" };

static void setup(void (*f)(struct oidset *st))
{
	struct oidset st = OIDSET_INIT;
	struct object_id oid;

	if (!check_int(oidset_size(&st), ==, 0))
		test_skip_all("OIDSET_INIT is broken");

	for (size_t i = 0; i < ARRAY_SIZE(hexes); i++) {
		if (get_oid_arbitrary_hex(hexes[i], &oid))
			return;
		if (!check_int(oidset_insert(&st, &oid), ==, 0))
			return;
	}
	check_int(oidset_size(&st), ==, ARRAY_SIZE(hexes));

	f(&st);
	oidset_clear(&st);
}

static void t_contains(struct oidset *st)
{
	struct object_id oid;

	for (size_t i = 0; i < ARRAY_SIZE(hexes); i++) {
		if (!get_oid_arbitrary_hex(hexes[i], &oid))
			check_int(oidset_contains(st, &oid), ==, 1);
	}

	if (!get_oid_arbitrary_hex("55", &oid))
		check_int(oidset_contains(st, &oid), ==, 0);
}

static void t_insert_from_set(struct oidset *st)
{
	struct oidset dest_st = OIDSET_INIT;

	oidset_insert_from_set(&dest_st, st);
	check_int(oidset_size(&dest_st), ==, oidset_size(st));
	t_contains(&dest_st);
	oidset_clear(&dest_st);
}

static void t_remove(struct oidset *st)
{
	struct object_id oid;

	if (!get_oid_arbitrary_hex("22", &oid)) {
		check_int(oidset_remove(st, &oid), ==, 1);
		check_int(oidset_size(st), ==, ARRAY_SIZE(hexes) - 1);
	}

	if (!get_oid_arbitrary_hex("cc", &oid)) {
		check_int(oidset_remove(st, &oid), ==, 1);
		check_int(oidset_size(st), ==, ARRAY_SIZE(hexes) - 2);
	}

	if (!get_oid_arbitrary_hex("55", &oid))
		check_int(oidset_remove(st, &oid), ==, 0);
}

static int input_contains(struct object_id *oid, char seen[])
{
	for (size_t i = 0; i < ARRAY_SIZE(hexes); i++) {
		struct object_id oid_input;
		if (get_oid_arbitrary_hex(hexes[i], &oid_input))
			return -1;
		if (oideq(&oid_input, oid)) {
			if (seen[i])
				return 2;
			seen[i] = 1;
			return 0;
		}
	}
	return 1;
}

static void t_iterate(struct oidset *st)
{
	struct oidset_iter iter;
	struct object_id *oid;
	char seen[ARRAY_SIZE(hexes)] = { 0 };
	int count = 0;

	oidset_iter_init(st, &iter);
	while ((oid = oidset_iter_next(&iter))) {
		int ret;
		if (!check_int((ret = input_contains(oid, seen)), ==, 0)) {
			switch (ret) {
			case -1:
				break; /* handled by get_oid_arbitrary_hex() */
			case 1:
				test_msg("obtained object_id was not given in the input\n"
					 "  object_id: %s", oid_to_hex(oid));
				break;
			case 2:
				test_msg("duplicate object_id detected\n"
					 "  object_id: %s", oid_to_hex(oid));
				break;
			}
		} else {
			count++;
		}
	}
	check_int(count, ==, ARRAY_SIZE(hexes));
	check_int(oidset_size(st), ==, ARRAY_SIZE(hexes));
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	TEST(setup(t_contains), "contains works");
	TEST(setup(t_insert_from_set), "insert from one set to another works");
	TEST(setup(t_remove), "remove works");
	TEST(setup(t_iterate), "iteration works");
	return test_done();
}
