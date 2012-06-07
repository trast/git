#!/bin/sh

test_description='test log -L'
. ./test-lib.sh

test_expect_success 'setup (import history)' '
	git fast-import < "$TEST_DIRECTORY"/t4211/history.export &&
	git reset --hard
'

canned_test () {
	test_expect_success "$1" "
		git log $1 >actual &&
		test_cmp \"\$TEST_DIRECTORY\"/t4211/expect.$2 actual
	"
}

canned_test "-L 4,12:a.c simple" simple-f
canned_test "-L 4,+9:a.c simple" simple-f
canned_test "-L '/long f/,/^}/:a.c' simple" simple-f

canned_test "-L '/main/,/^}/:a.c' simple" simple-main

canned_test "-L 1,+4:a.c simple" beginning-of-file

canned_test "-L 20:a.c simple" end-of-file

test_expect_success "-L 20,10000:a.c (bogus end)" '
	test_must_fail git log -L 20,10000:a.c simple 2>errors &&
	grep "has only.*lines" errors
'

canned_test "-L '/long f/',/^}/:a.c -L /main/,/^}/:a.c simple" two-ranges
canned_test "-L 24,+1:a.c simple" vanishes-early

canned_test "-L '/long f/,/^}/:b.c' move-support" move-support-f

test_done
