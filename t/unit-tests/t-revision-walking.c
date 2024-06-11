#include "test-lib.h"
#include "lib-repo.h"
#include "revision.h"

static void run_revision_walk(void)
{
	struct rev_info rev;
	struct commit *commit;
	const char *argv[] = { NULL, "--all", NULL };
	char *m[] = { "add b", "add a" };
	int argc = ARRAY_SIZE(argv) - 1, i = 0;
	struct strbuf sb = STRBUF_INIT;

	repo_init_revisions(the_repository, &rev, NULL);
	setup_revisions(argc, argv, &rev, NULL);
	if (prepare_revision_walk(&rev))
		die("revision walk setup failed");

	while ((commit = get_revision(&rev)) != NULL) {
		struct pretty_print_context ctx = { 0 };

		ctx.date_mode.type = DATE_NORMAL;
		repo_format_commit_message(the_repository, commit, "%s", &sb,
					   &ctx);
		check_str(sb.buf, m[i]);
		i++;
		strbuf_remove(&sb, 0, sb.len);
	}

	reset_revision_walk();
	release_revisions(&rev);
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	int ret = 0, new = 0;
	int skip = test__run_begin();
	if (!skip) {
		if (check_int((ret = test_setup_repo()), ==, 0)) {
			new = test_commit("add a", "a", "a");
			if (new == 0)
				ret = test_commit("add b", "b", "b");
		}
	}
	if(!test__run_end(skip, TEST_LOCATION(), "setup"))
		test_skip_all("failed to setup repo, ret: %d, new: %d", ret, new);
		
	TEST(run_revision_walk(), "run once");
	TEST(run_revision_walk(), "run twice");

	return test_done();
}
