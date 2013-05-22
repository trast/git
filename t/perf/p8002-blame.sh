#!/bin/sh

test_description="Tests blame performance"

. ./perf-lib.sh

test_perf_default_repo

# Pick 10 files to blame pseudo-randomly.  The sort key is the blob
# hash, so it is stable.
test_expect_success 'setup' '
	git ls-tree HEAD | grep ^100644 |
	sort -k 3 | head | cut -f 2 >filelist
'

test_perf 'blame' '
	while read -r name; do
		git blame HEAD -- "$name" >/dev/null
	done <filelist
'

test_perf 'blame -M' '
	while read -r name; do
		git blame -M HEAD -- "$name" >/dev/null
	done <filelist
'

test_perf 'blame -C' '
	while read -r name; do
		git blame -C HEAD -- "$name" >/dev/null
	done <filelist
'

test_perf 'blame -C -C' '
	while read -r name; do
		git blame -C -C HEAD -- "$name" >/dev/null
	done <filelist
'

test_perf 'blame -C -C -C' '
	while read -r name; do
		git blame -C -C -C HEAD -- "$name" >/dev/null
	done <filelist
'

test_done
