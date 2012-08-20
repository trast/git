#!/bin/sh

test_description='miscellaneous rev-list tests'

. ./test-lib.sh

test_expect_success setup '
	echo content1 >wanted_file &&
	echo content2 >unwanted_file &&
	git add wanted_file unwanted_file &&
	git commit -m one
'

test_expect_success 'rev-list --objects heeds pathspecs' '
	git rev-list --objects HEAD -- wanted_file >output &&
	grep wanted_file output &&
	! grep unwanted_file output
'

test_expect_success 'rev-list --objects with pathspecs and deeper paths' '
	mkdir foo &&
	>foo/file &&
	git add foo/file &&
	git commit -m two &&

	git rev-list --objects HEAD -- foo >output &&
	grep foo/file output &&

	git rev-list --objects HEAD -- foo/file >output &&
	grep foo/file output &&
	! grep unwanted_file output
'

test_expect_success 'rev-list --objects with pathspecs and copied files' '
	git checkout --orphan junio-testcase &&
	git rm -rf . &&

	mkdir two &&
	echo frotz >one &&
	cp one two/three &&
	git add one two/three &&
	test_tick &&
	git commit -m that &&

	ONE=$(git rev-parse HEAD:one)
	git rev-list --objects HEAD two >output &&
	grep "$ONE two/three" output &&
	! grep one output
'

make_some_commits () {
	msg=$1
	for i in $(seq 1 8); do
		echo $i >foo &&
		git add foo &&
		test_tick &&
		git commit -m"$i $msg"
	done
}

test_expect_success 'set up out-of-order timestamps' '
	test_commit base &&
	git checkout -b normal &&
	make_some_commits normal &&
	git checkout -b past base &&
	test_tick=$(($test_tick - 86400)) &&
	make_some_commits past
'

test_expect_success 'normal..past' '
	: >expected &&
	for i in 8 7 6 5 4 3 2 1; do
		echo "$i past" >>expected
	done &&
	git rev-list --pretty="%s" normal..past >actual &&
	grep -v commit actual >filtered &&
	test_cmp expected filtered
'

test_expect_failure 'past..normal' '
	: >expected &&
	for i in 8 7 6 5 4 3 2 1; do
		echo "$i normal" >>expected
	done &&
	git rev-list --pretty=%s past..normal >actual &&
	grep -v commit actual >filtered &&
	test_cmp expected filtered
'

test_done
