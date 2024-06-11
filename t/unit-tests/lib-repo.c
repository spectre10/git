#include "git-compat-util.h"
#include "lib-oid.h"
#include "lib-repo.h"
#include "setup.h"
#include "repository.h"
#include "run-command.h"
#include "environment.h"
#include "abspath.h"
#include "gettext.h"
#include "strbuf.h"
#include "refs.h"
#include "test-lib.h"

static const char *git_dir = NULL;

static int init_ref_backend(void)
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

int test_setup_repo_loc(const char *file)
{
	int ref_format = init_ref_backend();
	int hash_algo = init_hash_algo();
	int ret;
	struct strbuf path = STRBUF_INIT;

	if (ref_format == REF_STORAGE_FORMAT_UNKNOWN ||
	    hash_algo == GIT_HASH_UNKNOWN)
		return -1;

	file = make_relative(file);
	strbuf_add(&path, file + 2, strlen(file) - 2);
	strbuf_addstr(&path, ".trash");
	if (mkdir(path.buf, 0777))
		return -21;

	git_work_tree_cfg = real_pathdup(path.buf, 1);
	git_dir = xstrdup(path.buf);

	set_git_work_tree(git_work_tree_cfg);
	if (access(get_git_work_tree(), X_OK))
		die_errno(_("Cannot access work tree '%s'"),
			  get_git_work_tree());

	strbuf_addstr(&path, "/.git");
	ret = init_db(path.buf, NULL, "", hash_algo, ref_format, "main", 0, INIT_DB_QUIET);

	strbuf_release(&path);
	return ret;
}

int test_commit(const char *message, const char *file, const char *content)
{
	struct child_process detect = CHILD_PROCESS_INIT;
	struct strbuf buf = STRBUF_INIT;
	int ret = 0;
	detect.no_stdin = detect.no_stdout = detect.no_stderr = 1;
	detect.git_cmd = 1;

	if (file) {
		FILE *fptr;
		strbuf_addstr(&buf, git_dir);
		strbuf_addstr(&buf, "/");
		strbuf_addstr(&buf, file);
		fptr = fopen(buf.buf, "w");

		fprintf(fptr, "%s", content);
		fclose(fptr);
		strbuf_release(&buf);
		strvec_pushl(&detect.args, "-C", git_dir, "add", file, NULL);
		ret = run_command(&detect);
	}

	strvec_pushl(&detect.args, "-C", git_dir, "commit", "-m", message,
		     NULL);
	if (!file)
		strvec_push(&detect.args, "--allow-empty");

	if (ret == 0)
		check_int((ret = run_command(&detect)), ==, 0);
	return ret;
}

void test_delete_repo(void)
{
	if (git_dir) {
		struct child_process detect = CHILD_PROCESS_INIT;
		detect.no_stdin = detect.no_stdout = detect.no_stderr = 1;
		strvec_pushl(&detect.args, "rm", "-rf", git_dir, NULL);
		if (run_command(&detect) != 0)
			test_msg("failed delete");
	}
	free((char *)git_dir);
}
