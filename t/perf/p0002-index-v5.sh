#!/bin/sh

test_description="Tests index versions [23]/4/5"

. ./perf-lib.sh

test_perf_large_repo

test_expect_success 'convert to v3' '
	git update-index --index-version=3
'

test_perf 'v[23]: ls-files' '
	git ls-files >/dev/null
'

subdir=$(git ls-files | sed 's#/[^/]*$##' | grep -v '^$' | uniq | tail +30 | head -1)

test_perf "v[23]: grep nonexistent -- subdir" "
	test_must_fail git grep nonexistent -- $subdir >/dev/null
"

test_expect_success 'convert to v4' '
	git update-index --index-version=4
'

test_perf 'v4: ls-files' '
	git ls-files >/dev/null
'

test_perf "v4: grep nonexistent -- subdir" "
	test_must_fail git grep nonexistent -- $subdir >/dev/null
"

test_expect_success 'convert to v5' '
	git update-index --index-version=5
'

test_perf 'v5: ls-files' '
	git ls-files >/dev/null
'

test_perf "v5: grep nonexistent -- subdir" "
	test_must_fail git grep nonexistent -- $subdir >/dev/null
"

test_done
