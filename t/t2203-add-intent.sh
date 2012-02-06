#!/bin/sh

test_description='Intent to add'

. ./test-lib.sh

test_expect_success 'intent to add' '
	echo hello >file &&
	echo hello >elif &&
	git add -N file &&
	git add elif
'

test_expect_success 'check result of "add -N"' '
	git ls-files -s file >actual &&
	empty=$(git hash-object --stdin </dev/null) &&
	echo "100644 $empty 0	file" >expect &&
	test_cmp expect actual
'

test_expect_success 'intent to add is just an ordinary empty blob' '
	git add -u &&
	git ls-files -s file >actual &&
	git ls-files -s elif | sed -e "s/elif/file/" >expect &&
	test_cmp expect actual
'

test_expect_success 'intent to add does not clobber existing paths' '
	git add -N file elif &&
	empty=$(git hash-object --stdin </dev/null) &&
	git ls-files -s >actual &&
	! grep "$empty" actual
'

test_expect_success 'cannot commit with i-t-a entry' '
	test_tick &&
	git commit -a -m initial &&
	git reset --hard &&

	echo xyzzy >rezrov &&
	echo frotz >nitfol &&
	git add rezrov &&
	git add -N nitfol &&
	git commit -minitial
'

test_expect_success 'can commit tree with i-t-a entry' '
	git reset --hard HEAD^ &&
	echo xyzzy >rezrov &&
	echo frotz >nitfol &&
	git add rezrov &&
	git add -N nitfol &&
	git config commit.ignoreIntentToAdd true &&
	git commit -m initial &&
	git ls-tree -r HEAD >actual &&
	cat >expected <<EOF &&
100644 blob ce013625030ba8dba906f756967f9e9ca394464a	elif
100644 blob ce013625030ba8dba906f756967f9e9ca394464a	file
100644 blob cf7711b63209d0dbc2d030f7fe3513745a9880e4	rezrov
EOF
	test_cmp expected actual &&
	git config commit.ignoreIntentToAdd false &&
	git reset HEAD^
'

test_expect_success 'can commit with an unrelated i-t-a entry in index' '
	git reset --hard &&
	echo xyzzy >rezrov &&
	echo frotz >nitfol &&
	git add rezrov &&
	git add -N nitfol &&
	git commit -m partial rezrov
'

test_expect_success 'can "commit -a" with an i-t-a entry' '
	git reset --hard &&
	: >nitfol &&
	git add -N nitfol &&
	git commit -a -m all
'

test_done

