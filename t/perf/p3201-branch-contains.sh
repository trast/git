#!/bin/sh

test_description="Tests branch --contains performance"

. ./perf-lib.sh

test_perf_default_repo

test_perf 'branch --contains' '
	git branch --contains HEAD >/dev/null
'

test_done
