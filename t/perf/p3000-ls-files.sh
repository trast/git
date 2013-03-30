#!/bin/sh

test_description="git-grep performance in various modes"

. ./perf-lib.sh

test_perf_large_repo
test_checkout_worktree

test_perf 'ls-files' '
	git ls-files >/dev/null
'
test_perf 'ls-files -o' '
	git ls-files -o >/dev/null
'
test_perf 'ls-files --exclude-standard -o' '
	git ls-files --exclude-standard -o >/dev/null
'
test_done
