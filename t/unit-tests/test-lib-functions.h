#include "git-compat-util.h"
#include "hash-ll.h"

/*
 * converts arbitrary hex string to object_id.
 * For example, passing "abc12" will generate
 * "abc1222222222222222222222222222222222222" hex of length 40 for SHA-1 and
 * create object_id out of that. And similarly for SHA-256, it will generate the
 * hex of length 64 to create object_id.
 * WARNING: passing a string of length more than the hexsz of respective hash
 * algo will return -1 and a error message to stdout.
 *
 * should we do "abcabcabc" (t0064) instead of "abccccccccc" for "abc" ?
 * or "abc000000000" like in t0069
 */
#define get_oid_arbitrary(hex, hash_algo, oid) \
	get_oid_loc(hex, strlen(hex), hash_algo, oid)
int get_oid_loc(const char *s, size_t sz, int hash_algo, struct object_id *oid);
