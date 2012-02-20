#!/bin/sh

test_description="Test whether cache-tree is properly updated

Tests whether various commands properly update and/or rewrite the
cache-tree extension.
"
 . ./test-lib.sh

cmp_cache_tree () {
	test-dump-cache-tree >actual &&
	sed "s/$_x40/SHA/" <actual >filtered &&
	test_cmp "$1" filtered
}

# We don't bother with actually checking the SHA1:
# test-dump-cache-tree already verifies that all existing data is
# correct.
test_cache_tree () {
	printf "SHA  (%d entries, 1 subtrees)\n" $(git ls-files|wc -l) >expect &&
	printf "SHA sub/ (%d entries, 0 subtrees)\n" $(git ls-files sub|wc -l) >>expect &&
	cmp_cache_tree expect
}

test_invalid_cache_tree () {
	echo "invalid                                   (1 subtrees)" >expect &&
	printf "SHA #(ref)  (%d entries, 1 subtrees)\n" $(git ls-files|wc -l) >>expect &&
	printf "SHA sub/ (%d entries, 0 subtrees)\n" $(git ls-files sub|wc -l) >>expect &&
	cmp_cache_tree expect
}

test_no_cache_tree () {
	: >expect &&
	cmp_cache_tree expect
}

test_expect_failure 'initial commit has cache-tree' '
	mkdir sub &&
	echo bar > sub/bar &&
	git add sub/bar &&
	test_commit foo &&
	test_cache_tree
'

test_expect_success 'read-tree HEAD establishes cache-tree' '
	git read-tree HEAD &&
	test_cache_tree
'

test_expect_success 'git-add invalidates cache-tree' '
	test_when_finished "git reset --hard; git read-tree HEAD" &&
	echo "I changed this file" > foo &&
	git add foo &&
	test_invalid_cache_tree
'

test_expect_success 'update-index invalidates cache-tree' '
	test_when_finished "git reset --hard; git read-tree HEAD" &&
	echo "I changed this file" > foo &&
	git update-index --add foo &&
	test_invalid_cache_tree
'

test_expect_success 'write-tree establishes cache-tree' '
	test-scrap-cache-tree &&
	git write-tree &&
	test_cache_tree
'

test_expect_success 'test-scrap-cache-tree works' '
	git read-tree HEAD &&
	test-scrap-cache-tree &&
	test_no_cache_tree
'

test_expect_success 'second commit has cache-tree' '
	test_commit bar &&
	test_cache_tree
'

test_expect_success 'reset --hard gives cache-tree' '
	test-scrap-cache-tree &&
	git reset --hard &&
	test_cache_tree
'

test_expect_success 'reset --hard without index gives cache-tree' '
	rm -f .git/index &&
	git reset --hard &&
	test_cache_tree
'

test_expect_success 'checkout HEAD leaves cache-tree intact' '
	git read-tree HEAD &&
	git checkout HEAD &&
	test_cache_tree
'

# NEEDSWORK: only one of these two can succeed.  The second is there
# because it would be the better result.
test_expect_success 'checkout HEAD^ correctly invalidates cache-tree' '
	git checkout HEAD^ &&
	test_invalid_cache_tree
'

test_expect_failure 'checkout HEAD^ gives full cache-tree' '
	git checkout master &&
	git read-tree HEAD &&
	git checkout HEAD^ &&
	test_cache_tree
'

test_done
