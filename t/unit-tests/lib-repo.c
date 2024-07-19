#include "test-lib.h"
#include "lib-oid.h"
#include "lib-repo.h"
#include "setup.h"
#include "run-command.h"
#include "environment.h"
#include "abspath.h"
#include "gettext.h"
#include "strbuf.h"
#include "path.h"

/* Chris: how do we manage this variable better? (currently we must call
 * test_repo_cleanup() at the end of the test to free this var and delete the
 * trash dir) */
static const char *git_dir_path = NULL;

static int init_ref_format(void)
{
	static int format = -1;
	if (format < 0) {
		const char *format_env = getenv("GIT_TEST_DEFAULT_REF_FORMAT");
		format = format_env ? ref_storage_format_by_name(format_env) :
				      REF_STORAGE_FORMAT_FILES;

		if (!check_int(format, !=, REF_STORAGE_FORMAT_UNKNOWN))
			test_msg(
				"invalid GIT_TEST_DEFAULT_REF_FORMAT value ('%s')",
				format_env);
	}
	return format;
}

static int strbuf_get_bin_path(struct strbuf *bin_path)
{
	/* Chris: is there an existing method to get the root of the repo? */
	strbuf_getcwd(bin_path);
	strbuf_strip_suffix(bin_path, "t/unit-tests/bin");
	strbuf_strip_suffix(bin_path, "t");
	strbuf_addstr(bin_path, "bin-wrappers/git");
	if (!check_int(access(bin_path->buf, X_OK), ==, 0)) {
		test_msg("BUG: has Git been built?");
		return -1;
	}
	return 0;
}

static int test_repo_config(const char *key, const char *value)
{
	struct child_process config = CHILD_PROCESS_INIT;
	struct strbuf bin_path = STRBUF_INIT;
	int ret = 0;

	if (!strbuf_get_bin_path(&bin_path)) {
		config.no_stdin = config.no_stdout = 1;
		strvec_pushl(&config.args, bin_path.buf, "-C", git_dir_path,
			     "config", key, value, NULL);
		if (!check_int((ret = run_command(&config)), ==, 0))
			test_msg("failed to set user.name");
	}
	strbuf_release(&bin_path);
	return ret;
}

int test_setup_repo_loc(const char *file)
{
	int ref_format = init_ref_format();
	int hash_algo = init_hash_algo();
	int ret;
	struct strbuf path = STRBUF_INIT;

	if (!check_int(ref_format, !=, REF_STORAGE_FORMAT_UNKNOWN) ||
	    !check_int(hash_algo, !=, GIT_HASH_UNKNOWN))
		return -1;

	strbuf_getcwd(&path);
	strbuf_strip_suffix(&path, "/unit-tests/bin");

	/* Chris: is there a method obtaining the filename without any leading
	 * path? */
	file = remove_leading_path(file, "t/unit-tests");

	strbuf_addf(&path, "/unit-tests/%s", file);
	strbuf_strip_suffix(&path, ".c");
	strbuf_addf(&path, ".trash");

	/* check if the trash dir already exists and remove if it does */
	if (!access(path.buf, F_OK)) {
		struct child_process rm = CHILD_PROCESS_INIT;

		rm.no_stdin = rm.no_stdout = rm.no_stderr = 1;
		strvec_pushl(&rm.args, "rm", "-rf", path.buf, NULL);
		if (!check_int((ret = run_command(&rm)), ==, 0)) {
			test_msg("failed to delete existing trash directory: %s",
				 path.buf);
			return ret;
		}
	};

	if (!check_int((ret = mkdir(path.buf, 0777)), ==, 0)) {
		test_msg("failed to create the trash directory '%s': %s",
			 path.buf, strerror(errno));
		return ret;
	}

	git_work_tree_cfg = real_pathdup(path.buf, 1);
	git_dir_path = xstrdup(path.buf);

	set_git_work_tree(git_work_tree_cfg);
	if (access(get_git_work_tree(), X_OK))
		die_errno(_("Cannot access work tree '%s'"),
			  get_git_work_tree());

	strbuf_addstr(&path, "/.git");
	ret = init_db(path.buf, NULL, "", hash_algo, ref_format, "main", 0,
		      INIT_DB_QUIET);

	if (!ret) {
		check_int((ret = test_repo_config("user.name", "'A U Thor'")), ==, 0);
		check_int((ret |= test_repo_config("user.email", "author@example.com")), ==, 0);
	}
	strbuf_release(&path);
	return ret;
}

static int test_add(struct test_file *files, size_t n)
{
	struct child_process add_process = CHILD_PROCESS_INIT;
	struct strbuf bin_path = STRBUF_INIT;
	int ret = 0;

	for (size_t i = 0; i < n; i++) {
		if (!check(files[i].name != NULL) ||
		    !check(files[i].content != NULL)) {
			test_msg("BUG: name/content of the file not set");
			return -1;
		}
	}

	if (!strbuf_get_bin_path(&bin_path)) {
		for (size_t i = 0; i < n; i++) {
			strvec_pushl(&add_process.args, bin_path.buf, "-C",
				     git_dir_path, "add", files[i].name, NULL);
			if (!check_int((ret = run_command(&add_process)), ==,
				       0))
				test_msg("could not stage files");
		}
	}

	strbuf_release(&bin_path);
	return ret;
}

int test_commit(const struct test_commit_opts *commit)
{
	struct child_process commit_process = CHILD_PROCESS_INIT;
	struct strbuf buf = STRBUF_INIT, bin_path = STRBUF_INIT;
	int ret = 0;
	commit_process.no_stdin = commit_process.no_stdout = 1;

	if ((ret = strbuf_get_bin_path(&bin_path))) {
		strbuf_release(&bin_path);
		return ret;
	}
	for (int i = 0; i < commit->files_nr; i++) {
		FILE *fptr;
		strbuf_addf(&buf, "%s/%s", git_dir_path, commit->files[i].name);
		fptr = fopen_or_warn(buf.buf, "w");
		strbuf_release(&buf);

		if (fptr) {
			fprintf(fptr, "%s", commit->files[i].content);
			fclose(fptr);
			if (!check_int((ret = test_add(commit->files,
						       commit->files_nr)),
				       ==, 0))
				return ret;
		}
	}

	strvec_pushl(&commit_process.args, bin_path.buf, "-C", git_dir_path,
		     "commit", "-m", commit->message, NULL);
	if (commit->files_nr == 0)
		strvec_push(&commit_process.args, "--allow-empty");

	if (!check_int((ret = run_command(&commit_process)), ==, 0))
		test_msg("could not perform the commit");

	strbuf_release(&bin_path);
	return ret;
}

void test_repo_cleanup(void)
{
	if (git_dir_path) {
		struct child_process rm = CHILD_PROCESS_INIT;

		rm.no_stdin = rm.no_stdout = rm.no_stderr = 1;
		strvec_pushl(&rm.args, "rm", "-rf", git_dir_path, NULL);
		if (run_command(&rm) != 0)
			test_msg("failed delete");
	}
	free((char *)git_dir_path);
}
