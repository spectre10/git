#include "test-lib.h"
#include "hash-ll.h"
#include "hex.h"
#include "strbuf.h"

static void check_hash_data(const void *data, size_t data_length, const char *expected, int algo) {
	git_hash_ctx ctx;
	unsigned char hash[GIT_MAX_HEXSZ];
	const struct git_hash_algo *algop = &hash_algos[algo];

	if (!check(!!data)) {
    		test_msg("Error: No data provided when expecting: %s", expected);
    		return;
	}

	algop->init_fn(&ctx);
	algop->update_fn(&ctx, data, data_length);
	algop->final_fn(hash, &ctx);

	check_str(hash_to_hex_algop(hash, algop), expected);
}

/* Works with a NUL terminated string. Doesn't work if it should contain a NUL character. */
#define TEST_SHA1_STR(data, expected) \
    	TEST(check_hash_data(data, strlen(data), expected, GIT_HASH_SHA1), \
        	"SHA1 (%s) works", #data)

/* Only works with a literal string, useful when it contains a NUL character. */
#define TEST_SHA1_LITERAL(literal, expected) \
    	TEST(check_hash_data(literal, (sizeof(literal) - 1), expected, GIT_HASH_SHA1), \
        	"SHA1 (%s) works", #literal)

/* Works with a NUL terminated string. Doesn't work if it should contain a NUL character. */
#define TEST_SHA256_STR(data, expected) \
    	TEST(check_hash_data(data, strlen(data), expected, GIT_HASH_SHA256), \
        	"SHA256 (%s) works", #data)

/* Only works with a literal string, useful when it contains a NUL character. */
#define TEST_SHA256_LITERAL(literal, expected) \
    	TEST(check_hash_data(literal, (sizeof(literal) - 1), expected, GIT_HASH_SHA256), \
        	"SHA256 (%s) works", #literal)

int cmd_main(int argc, const char **argv) {
	struct strbuf aaaaaaaaaa_100000 = STRBUF_INIT;
	struct strbuf alphabet_100000 = STRBUF_INIT;

	strbuf_addstrings(&aaaaaaaaaa_100000, "aaaaaaaaaa", 100000);
	strbuf_addstrings(&alphabet_100000, "abcdefghijklmnopqrstuvwxyz", 100000);

	TEST_SHA1_STR("", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
	TEST_SHA1_STR("a", "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");
	TEST_SHA1_STR("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
	TEST_SHA1_STR("message digest", "c12252ceda8be8994d5fa0290a47231c1d16aae3");
	TEST_SHA1_STR("abcdefghijklmnopqrstuvwxyz", "32d10c7b8cf96570ca04ce37f2a19d84240d3a89");
	TEST_SHA1_STR(aaaaaaaaaa_100000.buf, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
	TEST_SHA1_LITERAL("blob 0\0", "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391");
	TEST_SHA1_LITERAL("blob 3\0abc", "f2ba8f84ab5c1bce84a7b441cb1959cfc7093b7f");
	TEST_SHA1_LITERAL("tree 0\0", "4b825dc642cb6eb9a060e54bf8d69288fbee4904");

	TEST_SHA256_STR("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	TEST_SHA256_STR("a", "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
	TEST_SHA256_STR("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	TEST_SHA256_STR("message digest", "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650");
	TEST_SHA256_STR("abcdefghijklmnopqrstuvwxyz", "71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73");
	TEST_SHA256_STR(aaaaaaaaaa_100000.buf, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	TEST_SHA256_STR(alphabet_100000.buf, "e406ba321ca712ad35a698bf0af8d61fc4dc40eca6bdcea4697962724ccbde35");
	TEST_SHA256_LITERAL("blob 0\0", "473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813");
	TEST_SHA256_LITERAL("blob 3\0abc", "c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6");
	TEST_SHA256_LITERAL("tree 0\0", "6ef19b41225c5369f1c104d45d8d85efa9b057b53b14b4b9b939dd74decc5321");

	strbuf_release(&aaaaaaaaaa_100000);
	strbuf_release(&alphabet_100000);

	return test_done();
}
