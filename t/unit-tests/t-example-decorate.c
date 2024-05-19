#include "test-lib.h"
#include "git-compat-util.h"
#include "object.h"
#include "decorate.h"
#include "repository.h"

struct test_vars {
	struct object_id one_oid, two_oid, three_oid;
	struct object *one, *two, *three;
	struct decoration n;
	int decoration_a, decoration_b;
	void *ret;
};

static void t_add(struct test_vars *vars)
{
	vars->one = lookup_unknown_object(the_repository, &vars->one_oid);
	vars->two = lookup_unknown_object(the_repository, &vars->two_oid);
	vars->ret = add_decoration(&vars->n, vars->one, &vars->decoration_a);
	if (!check(vars->ret == NULL))
		test_msg("when adding a brand-new object, NULL should be returned");
	vars->ret = add_decoration(&vars->n, vars->two, NULL);
	if (!check(vars->ret == NULL))
		test_msg("when adding a brand-new object, NULL should be returned");
}

static void t_readd(struct test_vars *vars)
{
	vars->ret = add_decoration(&vars->n, vars->one, NULL);
	if (!check(vars->ret == &vars->decoration_a))
		test_msg("when readding an already existing object, existing decoration should be returned");
	vars->ret = add_decoration(&vars->n, vars->two, &vars->decoration_b);
	if (!check(vars->ret == NULL))
		test_msg("when readding an already existing object, existing decoration should be returned");
}

static void t_lookup(struct test_vars *vars)
{
	vars->ret = lookup_decoration(&vars->n, vars->one);
	if (!check(vars->ret == NULL))
		test_msg("lookup should return added declaration");
	vars->ret = lookup_decoration(&vars->n, vars->two);
	if (!check(vars->ret == &vars->decoration_b))
		test_msg("lookup should return added declaration");
	vars->three = lookup_unknown_object(the_repository, &vars->three_oid);
	vars->ret = lookup_decoration(&vars->n, vars->three);
	if (!check(vars->ret == NULL))
		test_msg("lookup for unknown object should return NULL");
}

static void t_loop(struct test_vars *vars)
{
	int i, objects_noticed = 0;

	for (i = 0; i < vars->n.size; i++) {
		if (vars->n.entries[i].base)
			objects_noticed++;
	}
	if (!check(objects_noticed == 2))
		test_msg("should have 2 objects");
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	struct test_vars vars = {
		.one_oid = { { 1 } },
		.two_oid = { { 2 } },
		.three_oid = { { 3 } },
	};

	/* The struct must be zero-initialized. */
	memset(&vars.n, 0, sizeof(vars.n));

	TEST(t_add(&vars),
	     "Add 2 objects, one with a non-NULL decoration and one with a NULL decoration.");
	TEST(t_readd(&vars),
	     "When re-adding an already existing object, the old decoration is returned.");
	TEST(t_lookup(&vars),
	     "Lookup returns the added declarations, or NULL if the object was never added.");
	TEST(t_loop(&vars), "The user can also loop through all entries.");

	clear_decoration(&vars.n, NULL);

	return test_done();
}
