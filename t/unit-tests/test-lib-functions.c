#include "test-lib-functions.h"
#include "test-lib.h"
#include "strbuf.h"
#include "hex.h"

int get_oid_loc(const char *hex, size_t sz, int hash_algo,
		struct object_id *oid)
{
	int i, ret;
	struct strbuf buf = STRBUF_INIT;

	if (sz > hash_algos[hash_algo].hexsz + 1) {
		test_msg("hex string bigger than maximum allowed");
		return -1;
	}

	strbuf_add(&buf, hex, sz - 1);
	for (i = sz; i <= hash_algos[hash_algo].hexsz; i++)
		strbuf_addch(&buf, hex[sz - 1]);

	ret = get_oid_hex_algop(buf.buf, oid, &hash_algos[hash_algo]);
	strbuf_release(&buf);

	return ret;
}
