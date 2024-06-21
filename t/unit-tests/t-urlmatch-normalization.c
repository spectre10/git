#include "test-lib.h"
#include "urlmatch.h"
#include "strbuf.h"

static void check_url(const char *url, int ret)
{
	char *url1 = url_normalize(url, NULL);
	if (url1)
		check_int(ret, ==, 0);
	else
		check_int(ret, ==, 1);
	free(url1);
}

static void check_url_against(const char *url, const char *expect)
{
	char *url1 = url_normalize(url, NULL);
	if (!expect)
		check(url1 == NULL);
	else if (check(url1 != NULL))
		check_str(url1, expect);
	free(url1);
}

static void check_url_against_normalized(const char *url1, const char *url2,
					 size_t equal)
{
	char *url1_norm = url_normalize(url1, NULL);
	char *url2_norm = url_normalize(url2, NULL);
	if (equal)
		check_str(url1_norm, url2_norm);
	else
		check(strcmp(url1_norm, url2_norm));
	free(url1_norm);
	free(url2_norm);
}

static void check_url_from_file(const char *file, const char *expect)
{
	struct strbuf content = STRBUF_INIT, path = STRBUF_INIT;
	char *url;

	strbuf_getcwd(&path);
	strbuf_strip_suffix(&path, "/unit-tests/bin"); /* because 'unit-tests-test-tool' is run from 'bin' directory */
	strbuf_addf(&path, "/unit-tests/t-urlmatch-normalization/%s", file);

	if (!check_int(strbuf_read_file(&content, path.buf, 0), >, 0)) {
		test_msg("failed to read from file: %s", file);
		goto cleanup;
	}

	strbuf_trim_trailing_newline(&content);
	url = url_normalize(content.buf, NULL);
	if (!expect)
		check(url == NULL);
	else if (check(url != NULL))
		check_str(url, expect);

	free(url);
cleanup:
	strbuf_release(&content);
	strbuf_release(&path);
}

static void check_normalized_length(const char *url, size_t len)
{
	struct url_info info;
	char *url1 = url_normalize(url, &info);
	check_int(info.url_len, ==, len);
	free(url1);
}

/* Note that only file: URLs should be allowed without a host */

static void t_url_scheme(void)
{
	check_url("", 1);
	check_url("_", 1);
	check_url("scheme", 1);
	check_url("scheme:", 1);
	check_url("scheme:/", 1);
	check_url("scheme://", 1);
	check_url("file", 1);
	check_url("file:", 1);
	check_url("file:/", 1);
	check_url("file://", 0);
	check_url("://acme.co", 1);
	check_url("x_test://acme.co", 1);
	check_url("-test://acme.co", 1);
	check_url("0test://acme.co", 1);
	check_url("+test://acme.co", 1);
	check_url(".test://acme.co", 1);
	check_url("schem%6e://", 1);
	check_url("x-Test+v1.0://acme.co", 0);
	check_url_against("AbCdeF://x.Y", "abcdef://x.y/");
}

static void t_url_authority(void)
{
	check_url("scheme://user:pass@", 1);
	check_url("scheme://?", 1);
	check_url("scheme://#", 1);
	check_url("scheme:///", 1);
	check_url("scheme://:", 1);
	check_url("scheme://:555", 1);
	check_url("file://user:pass@", 0);
	check_url("file://?", 0);
	check_url("file://#", 0);
	check_url("file:///", 0);
	check_url("file://:", 0);
	check_url("file://:555", 1);
	check_url("scheme://user:pass@host", 0);
	check_url("scheme://@host", 0);
	check_url("scheme://%00@host", 0);
	check_url("scheme://%%@host", 1);
	check_url("scheme://host_", 0);
	check_url("scheme://user:pass@host/", 0);
	check_url("scheme://@host/", 0);
	check_url("scheme://host/", 0);
	check_url("scheme://host?x", 0);
	check_url("scheme://host#x", 0);
	check_url("scheme://host/@", 0);
	check_url("scheme://host?@x", 0);
	check_url("scheme://host#@x", 0);
	check_url("scheme://[::1]", 0);
	check_url("scheme://[::1]/", 0);
	check_url("scheme://hos%41/", 1);
	check_url("scheme://[invalid....:/", 0);
	check_url("scheme://invalid....:]/", 0);
	check_url("scheme://invalid....:[/", 1);
	check_url("scheme://invalid....:[", 1);
}

static void t_url_port(void)
{
	check_url("xyz://q@some.host:", 0);
	check_url("xyz://q@some.host:456/", 0);
	check_url("xyz://q@some.host:0", 1);
	check_url("xyz://q@some.host:0000000", 1);
	check_url("xyz://q@some.host:0000001?", 0);
	check_url("xyz://q@some.host:065535#", 0);
	check_url("xyz://q@some.host:65535", 0);
	check_url("xyz://q@some.host:65536", 1);
	check_url("xyz://q@some.host:99999", 1);
	check_url("xyz://q@some.host:100000", 1);
	check_url("xyz://q@some.host:100001", 1);
	check_url("http://q@some.host:80", 0);
	check_url("https://q@some.host:443", 0);
	check_url("http://q@some.host:80/", 0);
	check_url("https://q@some.host:443?", 0);
	check_url("http://q@:8008", 1);
	check_url("http://:8080", 1);
	check_url("http://:", 1);
	check_url("xyz://q@some.host:456/", 0);
	check_url("xyz://[::1]:456/", 0);
	check_url("xyz://[::1]:/", 0);
	check_url("xyz://[::1]:000/", 1);
	check_url("xyz://[::1]:0%300/", 1);
	check_url("xyz://[::1]:0x80/", 1);
	check_url("xyz://[::1]:4294967297/", 1);
	check_url("xyz://[::1]:030f/", 1);
}

static void t_url_port_normalization(void)
{
	check_url_against("http://x:800", "http://x:800/");
	check_url_against("http://x:0800", "http://x:800/");
	check_url_against("http://x:00000800", "http://x:800/");
	check_url_against("http://x:065535", "http://x:65535/");
	check_url_against("http://x:1", "http://x:1/");
	check_url_against("http://x:80", "http://x/");
	check_url_against("http://x:080", "http://x/");
	check_url_against("http://x:000000080", "http://x/");
	check_url_against("https://x:443", "https://x/");
	check_url_against("https://x:0443", "https://x/");
	check_url_against("https://x:000000443", "https://x/");
}

static void t_url_general_escape(void)
{
	check_url("http://x.y?%fg", 1);
	check_url_against("X://W/%7e%41^%3a", "x://w/~A%5E%3A");
	check_url_against("X://W/:/?#[]@", "x://w/:/?#[]@");
	check_url_against("X://W/$&()*+,;=", "x://w/$&()*+,;=");
	check_url_against("X://W/'\''", "x://w/'\''");
	check_url_against("X://W?'!'", "x://w/?'!'"); // TODO
}

static void t_url_high_bit(void)
{
#if defined(__MINGW32__) || defined(__MINGW64__)
	test_skip("skipping non-windows specific tests");
	return;
#endif

	check_url_from_file("url-1",
			    "x://q/%01%02%03%04%05%06%07%08%0E%0F%10%11%12");
	check_url_from_file("url-2",
			    "x://q/%13%14%15%16%17%18%19%1B%1C%1D%1E%1F%7F");
	check_url_from_file("url-3",
			    "x://q/%80%81%82%83%84%85%86%87%88%89%8A%8B%8C%8D%8E%8F");
	check_url_from_file("url-4",
			    "x://q/%90%91%92%93%94%95%96%97%98%99%9A%9B%9C%9D%9E%9F");
	check_url_from_file("url-5",
			    "x://q/%A0%A1%A2%A3%A4%A5%A6%A7%A8%A9%AA%AB%AC%AD%AE%AF");
	check_url_from_file("url-6",
			    "x://q/%B0%B1%B2%B3%B4%B5%B6%B7%B8%B9%BA%BB%BC%BD%BE%BF");
	check_url_from_file("url-7",
			    "x://q/%C0%C1%C2%C3%C4%C5%C6%C7%C8%C9%CA%CB%CC%CD%CE%CF");
	check_url_from_file("url-8",
			    "x://q/%D0%D1%D2%D3%D4%D5%D6%D7%D8%D9%DA%DB%DC%DD%DE%DF");
	check_url_from_file("url-9",
			    "x://q/%E0%E1%E2%E3%E4%E5%E6%E7%E8%E9%EA%EB%EC%ED%EE%EF");
	check_url_from_file("url-10",
			    "x://q/%F0%F1%F2%F3%F4%F5%F6%F7%F8%F9%FA%FB%FC%FD%FE%FF");
}

static void t_url_utf8_escape(void)
{
	check_url_from_file("url-11",
			    "x://q/%C2%80%DF%BF%E0%A0%80%EF%BF%BD%F0%90%80%80%F0%AF%BF%BD");
}

static void t_url_username_pass(void)
{
	check_url_against("x://%41%62(^):%70+d@foo", "x://Ab(%5E):p+d@foo/");
}

static void t_url_length(void)
{
	check_normalized_length("Http://%4d%65:%4d^%70@The.Host", 25);
	check_normalized_length("http://%41:%42@x.y/%61/", 17);
	check_normalized_length("http://@x.y/^", 15);
}

static void t_url_dots(void)
{
	check_url_against("x://y/.", "x://y/");
	check_url_against("x://y/./", "x://y/");
	check_url_against("x://y/a/.", "x://y/a");
	check_url_against("x://y/a/./", "x://y/a/");
	check_url_against("x://y/.?", "x://y/?");
	check_url_against("x://y/./?", "x://y/?");
	check_url_against("x://y/a/.?", "x://y/a?");
	check_url_against("x://y/a/./?", "x://y/a/?");
	check_url_against("x://y/a/./b/.././../c", "x://y/c");
	check_url_against("x://y/a/./b/../.././c/", "x://y/c/");
	check_url_against("x://y/a/./b/.././../c/././.././.", "x://y/");
	check_url("x://y/a/./b/.././../c/././.././..", 1);
	check_url_against("x://y/a/./?/././..", "x://y/a/?/././..");
	check_url_against("x://y/%2e/", "x://y/");
	check_url_against("x://y/%2E/", "x://y/");
	check_url_against("x://y/a/%2e./", "x://y/");
	check_url_against("x://y/b/.%2E/", "x://y/");
	check_url_against("x://y/c/%2e%2E/", "x://y/");
}

/*
 * http://@foo specifies an empty user name but does not specify a password
 * http://foo  specifies neither a user name nor a password
 * So they should not be equivalent
 */
static void t_url_equivalents(void)
{
	check_url_against_normalized("httP://x", "Http://X/", 1);
	check_url_against_normalized("Http://%4d%65:%4d^%70@The.Host", "hTTP://Me:%4D^p@the.HOST:80/", 1);
	check_url_against_normalized("https://@x.y/^", "httpS://x.y:443/^", 0);
	check_url_against_normalized("https://@x.y/^", "httpS://@x.y:0443/^", 1);
	check_url_against_normalized("https://@x.y/^/../abc", "httpS://@x.y:0443/abc", 1);
	check_url_against_normalized("https://@x.y/^/..", "httpS://@x.y:0443/", 1);
}

int cmd_main(int argc UNUSED, const char **argv UNUSED)
{
	TEST(t_url_scheme(), "url scheme");
	TEST(t_url_authority(), "url authority");
	TEST(t_url_port(), "url port checks");
	TEST(t_url_port_normalization(), "url port normalization");
	TEST(t_url_general_escape(), "url general escapes");
	TEST(t_url_high_bit(), "url high-bit escapes");
	TEST(t_url_utf8_escape(), "url utf8 escapes");
	TEST(t_url_username_pass(), "url username/password escapes");
	TEST(t_url_length(), "url normalized lengths");
	TEST(t_url_dots(), "url . and .. segments");
	TEST(t_url_equivalents(), "url equivalents");
	return test_done();
}
