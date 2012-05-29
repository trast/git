#!/bin/sh

test_description="Tests git-rebase performance"

. ./perf-lib.sh

test_perf_default_repo

test_expect_success 'find a long run of linear history' '
	git rev-list --topo-order --parents --all |
	perl -e '\''$maxL = 0; $maxcommit = undef;
		while (<>) {
			chomp;
			@parents = split;
			$commit = shift @parents;
			if ($L{$commit} > $maxL) {
				$maxL = $L{$commit};
				$maxcommit = $commit;
			}
			if (1 == scalar @parents
				&& $L{$commit} >= $L{$parents[0]}) {
				$L{$parents[0]} = $L{$commit}+1;
				$C{$parents[0]} = $commit;
			}
		}
		$cur = $maxcommit;
		while (defined $C{$cur}) {
			$cur = $C{$cur};
		}
		if ($maxL > 50) {
			$maxL = 50;
		}
		print "$cur~$maxL\n$cur\n";
	'\'' >rebase-args &&
	base_rev=$(sed -n 1p rebase-args) &&
	tip_rev=$(sed -n 2p rebase-args) &&
	git checkout -f -b work $tip_rev
'

export base_rev tip_rev

test_expect_success 'verify linearity' '
	git rev-list --parents $base_rev.. >list &&
	! grep "[0-9a-f]+ [0-9a-f]+ [0-9a-f+].*" list
'

test_expect_success 'disable rerere' '
	git config rerere.enabled false
'

test_perf 'rebase -f (rerere off)' '
	git rebase -f $base_rev
'

test_perf 'rebase -m -f (rerere off)' '
	git rebase -m -f $base_rev
'

test_perf 'rebase -i -f (rerere off)' '
	GIT_EDITOR=: git rebase -i -f $base_rev
'

test_perf 'rebase -i -m -f (rerere off)' '
	GIT_EDITOR=: git rebase -i -m -f $base_rev
'

test_expect_success 'enable rerere and prime it' '
	git config rerere.enabled true &&
	git rebase -f $base_rev &&
	git rebase -m -f $base_rev &&
	GIT_EDITOR=: git rebase -i -f $base_rev &&
	GIT_EDITOR=: git rebase -i -m -f $base_rev
'

test_perf 'rebase -f (rerere ON)' '
	git rebase -f $base_rev
'

test_perf 'rebase -m -f (rerere ON)' '
	git rebase -m -f $base_rev
'

test_perf 'rebase -i -f (rerere ON)' '
	GIT_EDITOR=: git rebase -i -f $base_rev
'

test_perf 'rebase -i -m -f (rerere ON)' '
	GIT_EDITOR=: git rebase -i -m -f $base_rev
'

test_done
