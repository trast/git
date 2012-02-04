#!/bin/sh

test_description='git column'
. ./test-lib.sh

test_expect_success 'setup' '
	cat >lista <<\EOF
one
two
three
four
five
six
seven
eight
nine
ten
eleven
EOF
'

test_expect_success 'never' '
	git column --mode=never <lista >actual &&
	test_cmp lista actual
'

test_done
