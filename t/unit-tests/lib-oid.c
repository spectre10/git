#include "test-lib.h"
#include "lib-oid.h"
#include "strbuf.h"
#include "hex.h"

static int get_oid_arbitrary_hex_algop(const char *hex, size_t sz,
				       int hash_algo, struct object_id *oid)
{
	int ret;
	struct strbuf buf = STRBUF_INIT;

	if (!check(sz < hash_algos[hash_algo].hexsz)) {
		test_msg("BUG: hex string bigger than maximum allowed");
		return -1;
	}

	strbuf_add(&buf, hex, sz);
	strbuf_addchars(&buf, '0', hash_algos[hash_algo].hexsz - sz);

	ret = get_oid_hex_algop(buf.buf, oid, &hash_algos[hash_algo]);
	strbuf_release(&buf);

	return ret;
}

int get_oid_arbitrary_hex(const char *hex, struct object_id *oid)
{
	const char *algo_name = getenv("GIT_TEST_DEFAULT_HASH");
	int hash_algo = GIT_HASH_SHA1;

	if (algo_name)
		hash_algo = hash_algo_by_name(algo_name);

	if (!check(hash_algo != GIT_HASH_UNKNOWN)) {
		test_msg("BUG: invalid GIT_TEST_DEFAULT_HASH value");
		return -1;
	}
	return get_oid_arbitrary_hex_algop(hex, strlen(hex), hash_algo, oid);
}
