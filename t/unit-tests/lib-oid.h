#ifndef LIB_OID_H
#define LIB_OID_H

#include "git-compat-util.h"
#include "hash-ll.h"

/*
 * converts arbitrary hex string to object_id.
 * For example, passing "abc12" will generate
 * "abc1200000000000000000000000000000000000" hex of length 40 for SHA-1 and
 * create object_id out of that. And similarly for SHA-256, it will generate the
 * hex of length 64 to create object_id.
 * WARNING: passing a string of length more than the hexsz of respective hash
 * algo is not allowed.
 */
#define get_oid_arbitrary_hex(hex, oid)                                    \
	const char *algo = getenv("GIT_TEST_DEFAULT_HASH");                \
	get_oid_arbitrary_hex_loc(hex, strlen(hex),                        \
				  hash_algo_by_name(algo ? algo : "sha1"), \
				  oid)
int get_oid_arbitrary_hex_loc(const char *s, size_t sz, int hash_algo,
			      struct object_id *oid);

#endif /* LIB_OID_H */
