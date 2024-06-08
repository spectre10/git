#include "test-lib.h"
#include "oid-array.h"
#include "hex.h"

static void test(void)
{
	struct oid_array a = OID_ARRAY_INIT;
	struct object_id oid;
	get_oid_hex_any("8800000000000000000000000000000000000000", &oid);
	oid_array_append(&a, &oid);
	get_oid_hex_any("4400000000000000000000000000000000000000", &oid);
	oid_array_append(&a, &oid);
	get_oid_hex_any("aa00000000000000000000000000000000000000", &oid);
	oid_array_append(&a, &oid);
	get_oid_hex_any("5500000000000000000000000000000000000000", &oid);
	oid_array_append(&a, &oid);

	check_int(a.nr, ==, 4);
	check_int(oid_array_lookup(&a, &oid), ==, 1);

	oid_array_clear(&a);
}

int cmd_main(int argc, const char **argv)
{
	TEST(test(), "test");
	return test_done();
}
