#include "reftable/blocksource.h"
#include "reftable/reader.h"
#include "reftable/refname.h"
#include "reftable/reftable-error.h"
#include "reftable/reftable-writer.h"
#include "test-lib.h"

static void test_conflict(char *refname, char *add, char *del, int error_code)
{
	struct reftable_write_options opts = { 0 };
	struct strbuf buf = STRBUF_INIT;
	struct reftable_writer *w =
		reftable_new_writer(&strbuf_add_void, &noop_flush, &buf, &opts);
	struct reftable_ref_record rec = {
		.refname = refname,
		.value_type = REFTABLE_REF_SYMREF,
		.value.symref = "destination", /* make sure it's not a symref.
						*/
		.update_index = 1,
	};
	int err;
	struct reftable_block_source source = { NULL };
	struct reftable_reader *rd = NULL;
	struct reftable_table tab = { NULL };
	struct modification mod = { { NULL } };
	reftable_writer_set_limits(w, 1, 1);

	err = reftable_writer_add_ref(w, &rec);
	check_int(err, ==, 0);

	err = reftable_writer_close(w);
	check_int(err, ==, 0);
	reftable_writer_free(w);

	block_source_from_strbuf(&source, &buf);
	err = reftable_new_reader(&rd, &source, "filename");
	check_int(err, ==, 0);

	reftable_table_from_reader(&tab, rd);
	mod.tab = tab;

	if (add) {
		mod.add = &add;
		mod.add_len = 1;
	}
	if (del) {
		mod.del = &del;
		mod.del_len = 1;
	}

	err = modification_validate(&mod);
	check_int(err, ==, error_code);

	reftable_reader_free(rd);
	strbuf_release(&buf);
}

int cmd_main(int argc, const char **argv)
{
	TEST(test_conflict("a/b", "a/b/c", NULL, REFTABLE_NAME_CONFLICT),
	     "conflict");
	TEST(test_conflict("a/b", "b", NULL, 0), "conflict");
	TEST(test_conflict("a/b", "a", NULL, REFTABLE_NAME_CONFLICT),
	     "conflict");
	TEST(test_conflict("a/b", "a", "a/b", 0), "conflict");
	TEST(test_conflict("a/b", "p/", NULL, REFTABLE_REFNAME_ERROR),
	     "conflict");
	TEST(test_conflict("a/b", "p//q", NULL, REFTABLE_REFNAME_ERROR),
	     "conflict");
	TEST(test_conflict("a/b", "p/./q", NULL, REFTABLE_REFNAME_ERROR),
	     "conflict");
	TEST(test_conflict("a/b", "p/../q", NULL, REFTABLE_REFNAME_ERROR),
	     "conflict");
	TEST(test_conflict("a/b", "a/b/c", "a/b", 0), "conflict");
	TEST(test_conflict("a/b", NULL, "a//b", 0), "conflict");
	return test_done();
}
