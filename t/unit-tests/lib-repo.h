#ifndef LIB_REPO_H

#define test_setup_repo() test_setup_repo_loc(__FILE__)
int test_setup_repo_loc(const char *file);

int test_commit(const char *message, const char *file, const char *content);

void test_delete_repo(void);

#endif // !LIB_REPO_H
