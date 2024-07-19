#ifndef LIB_REPO_H

struct test_file {
	const char *name;
	const char *content;
};

struct test_commit_opts {
	const char *message;
	size_t files_nr;
	struct test_file *files;
};

#define test_setup_repo() test_setup_repo_loc(__FILE__)
int test_setup_repo_loc(const char *file);

int test_commit(const struct test_commit_opts *commit);

void test_repo_cleanup(void);

#endif /* LIB_REPO_H */
