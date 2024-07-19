#define USE_THE_REPOSITORY_VARIABLE

#include "test-lib.h"
#include "lib-repo.h"
#include "revision.h"

static const struct test_commit_opts commits[2] = {
	{
		.message = "add a",
		/* Chris: how do we make sure files_nr and ARRAY_SIZE(files) match? */
		.files_nr = 1,
		.files =
			(struct test_file[]){ { .name = "a", .content = "a" } },
	},
	{
		.message = "add b",
		.files_nr = 1,
		.files =
			(struct test_file[]){ { .name = "b", .content = "b" } },
	}
};

static void check_commit(struct commit *commit, char seen[], int i)
{
	struct strbuf sb = STRBUF_INIT;
	struct pretty_print_context ctx = { 0 };

	ctx.date_mode.type = DATE_NORMAL;
	repo_format_commit_message(the_repository, commit, "%s", &sb, &ctx);
	if (check_str(sb.buf, commits[i].message))
		seen[i] = 1;

	strbuf_release(&sb);
}

static void run_revision_walk(void)
{
	struct rev_info rev;
	struct commit *commit;
	const char *argv[] = { NULL, "--all", NULL };
	int argc = ARRAY_SIZE(argv) - 1;
	char seen[ARRAY_SIZE(commits)] = { 0 };
	int count = 0;

	repo_init_revisions(the_repository, &rev, NULL);
	setup_revisions(argc, argv, &rev, NULL);
	if (!check_int(prepare_revision_walk(&rev), ==, 0)) {
		test_msg("revision walk setup failed");
		return;
	}

	while ((commit = get_revision(&rev)) != NULL) {
		check_commit(commit, seen, ARRAY_SIZE(commits) - count - 1);
		count++;
	}
	check_int(count, ==, ARRAY_SIZE(commits));

	for (int i = 0; i < ARRAY_SIZE(commits); i++) {
		if (!check_int(seen[i], ==, 1))
			test_msg("following commit was not iterated over: '%s'",
				 commits[i].message);
	}

	reset_revision_walk();
	release_revisions(&rev);
}

static void setup(void)
{
	if (!check_int(test_setup_repo(), ==, 0))
		return;
	for (int i = 0; i < ARRAY_SIZE(commits); i++)
		if (!check_int(test_commit(&commits[i]), ==, 0))
			return;
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	if (!TEST(setup(), "setup"))
		test_skip_all("failed setup");
	TEST(run_revision_walk(), "revision walk works");
	TEST(run_revision_walk(), "revision walk works the second time");
	return test_done();
}
