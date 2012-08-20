#!/bin/sh

test_description="Tests history walking performance"

. ./perf-lib.sh

test_perf_default_repo

test_perf 'rev-list --all' '
	git rev-list --all >/dev/null
'

test_perf 'rev-list -50 --all' '
	git rev-list -50 --all >/dev/null
'

test_perf 'rev-list --topo-order --all' '
	git rev-list --topo-order --all >/dev/null
'

test_perf 'rev-list --topo-order -50 --all' '
	git rev-list --topo-order -50 --all >/dev/null
'

test_perf 'rev-list --all --objects' '
	git rev-list --all --objects >/dev/null
'

test_done
