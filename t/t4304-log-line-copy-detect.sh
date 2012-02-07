#!/bin/sh
#
# Copyright (c) 2010 Bo Yang
#

test_description='Test git log -L with -C'

. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

cat >path0 <<\EOF
void func()
{
	int a = 0;
	int b = 1;
	int c;
	c = a + b;
}
EOF

cat >path1 <<\EOF
void output()
{
	printf("hello world");
}
EOF

test_expect_success 'add path0/path1 and commit.' '
	git add path0 path1 &&
	git commit -m "Base commit"
'

cat >path1 <<\EOF
void output()
{
	int d = 3;
	int e = 5;
	printf("hello world");
	printf("bye!");
}
EOF

test_expect_success 'Change the some lines of path1.' '
	git add path1 &&
	git commit -m "Change some lines of path1"
'

cat >path2 <<\EOF
void comb()
{
	int a = 0;
	int b = 1;
	int c;
	c = a + b;
	int d = 3;
	int e = 5;
	printf("hello world");
	printf("bye!");
}
EOF

test_expect_success 'Move two functions into one in path2' '
	git add path2 &&
	git rm path0 path1 &&
	git commit -m "Move two functions into path2"
'

cat >path2 <<\EOF
void comb()
{
	int a = 0;
	int b = 1;
	int c;
	c = a + b;
	printf("hello world");
	printf("bye!");
}
EOF

test_expect_success 'Final change of path2.' '
	git add path2 &&
	git commit -m "Final change of path2"
'

sed -e 's/Q/ /g' -e 's/#$//' >expected-no-C <<\EOF
* Final change of path2
| #
| diff --git a/path2 b/path2
| index ca6a800..b744a93 100644
| --- a/path2
| +++ b/path2
| @@ -1,11 +1,9 @@
|  void comb()
|  {
|QQ	int a = 0;
|QQ	int b = 1;
|QQ	int c;
|QQ	c = a + b;
| -	int d = 3;
| -	int e = 5;
|QQ	printf("hello world");
|QQ	printf("bye!");
|  }
|  #
* Move two functions into path2
  #
  diff --git a/path2 b/path2
  new file mode 100644
  index 0000000..ca6a800
  --- /dev/null
  +++ b/path2
  @@ -0,0 +1,11 @@
  +void comb()
  +{
  +	int a = 0;
  +	int b = 1;
  +	int c;
  +	c = a + b;
  +	int d = 3;
  +	int e = 5;
  +	printf("hello world");
  +	printf("bye!");
  +}
EOF

sed -e 's/Q/ /g' -e 's/#$//' >expected-C <<\EOF
* Final change of path2
| #
| diff --git a/path2 b/path2
| index ca6a800..b744a93 100644
| --- a/path2
| +++ b/path2
| @@ -1,11 +1,9 @@
|  void comb()
|  {
|QQ	int a = 0;
|QQ	int b = 1;
|QQ	int c;
|QQ	c = a + b;
| -	int d = 3;
| -	int e = 5;
|QQ	printf("hello world");
|QQ	printf("bye!");
|  }
|  #
* Move two functions into path2
| #
| diff --git a/path2 b/path2
| new file mode 100644
| index 0000000..ca6a800
| --- /dev/null
| +++ b/path2
| @@ -0,0 +1,11 @@
|  void comb()
|  {
|QQ	int a = 0;
|QQ	int b = 1;
|QQ	int c;
|QQ	c = a + b;
|QQ	int d = 3;
|QQ	int e = 5;
|QQ	printf("hello world");
|QQ	printf("bye!");
|  }
|  #
* Change some lines of path1
| #
| diff --git a/path1 b/path1
| index 52be2a5..bf3a80f 100644
| --- a/path1
| +++ b/path1
| @@ -2,3 +2,6 @@
|  {
| +	int d = 3;
| +	int e = 5;
|QQ	printf("hello world");
| +	printf("bye!");
|  }
|  #
* Base commit
  #
  diff --git a/path1 b/path1
  new file mode 100644
  index 0000000..52be2a5
  --- /dev/null
  +++ b/path1
  @@ -0,0 +2,3 @@
  +{
  +	printf("hello world");
  +}
  diff --git a/path0 b/path0
  new file mode 100644
  index 0000000..fb33939
  --- /dev/null
  +++ b/path0
  @@ -0,0 +2,5 @@
  +{
  +	int a = 0;
  +	int b = 1;
  +	int c;
  +	c = a + b;
EOF

test_expect_success 'Show the line level log of path2' '
	git log --pretty=format:%s%n%b --graph -L /comb/,/^}/:path2 > current-no-C
'

test_expect_success 'validate the path2 output.' '
	test_cmp current-no-C expected-no-C
'

test_expect_success 'Show the line level log of path2 with -C' '
	git log --pretty=format:%s%n%b --graph -C -L /comb/,/^}/:path2 > current-C
'

test_expect_failure 'validate the path2 output.' '
	test_cmp current-C expected-C
'

test_done
