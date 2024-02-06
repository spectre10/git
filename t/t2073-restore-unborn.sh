#!/bin/sh

test_description='restore --staged should work on unborn branch'
. ./test-lib.sh

test_expect_success 'explicitly naming HEAD on unborn should fail' '
	echo a >foo &&
	echo b >bar &&
	git add foo bar &&
	test_must_fail git restore --staged --source=HEAD .
'

test_expect_success 'restore --staged .' '
	rm .git/index &&
	git add foo bar &&
	git restore --staged . &&
	git diff --cached --name-only >actual &&
	test_must_be_empty actual
'

test_expect_success 'restore --staged $file' '
	rm .git/index &&
	git add foo bar &&
	git restore --staged foo &&
	git diff --cached --name-only >actual &&
	echo bar >expected &&
	test_cmp expected actual
'

test_expect_success 'restore -p --staged' '
	rm .git/index &&
	git add foo bar &&
	test_write_lines y n | git restore -p --staged >output &&
	git diff --cached --name-only >actual &&
	echo foo >expected &&
	test_cmp expected actual &&
	test_grep "Unstage" output
'

test_done
