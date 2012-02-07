#!/bin/sh
#
# Copyright (c) 2010 Bo Yang
#

test_description='Test git log -L with code movement'

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

void output()
{
	printf("hello world");
}
EOF

test_expect_success 'add path0 and commit.' '
	git add path0 &&
	git commit -m "Base commit"
'

cat >path0 <<\EOF
void func()
{
	int a = 0;
	int b = 1;
	int c;
	c = a + b;
}

void output()
{
	int d = 3;
	int e = 5;
	printf("hello world");
	printf("bye!");
}
EOF

test_expect_success 'Change the some lines of path0.' '
	git add path0 &&
	git commit -m "Change some lines of path0"
'

cat >path0 <<\EOF
void func()
{
	int a = 0;
	int b = 1;
	int c;
	c = a + b;
}

void output()
{
	int d = 3;
	int e = 5;
	printf("hello world");
	printf("bye!");
}

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

test_expect_success 'Move two functions into one' '
	git add path0 &&
	git commit -m "Move two functions into one"
'

cat >path0 <<\EOF
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

test_expect_success 'Final change of path0.' '
	git add path0 &&
	git commit -m "Final change of path0"
'

sed -e 's/Q/ /g' -e 's/#$//' >expected-no-M <<\EOF
* Final change of path0
| #
| diff --git a/path0 b/path0
| index 495f978..b744a93 100644
| --- a/path0
| +++ b/path0
| @@ -17,11 +1,9 @@
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
* Move two functions into one
  #
  diff --git a/path0 b/path0
  index cd42622..495f978 100644
  --- a/path0
  +++ b/path0
  @@ -0,0 +17,11 @@
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

sed -e 's/Q/ /g' -e 's/#$//' >expected-M <<\EOF
* Final change of path0
| #
| diff --git a/path0 b/path0
| index 495f978..b744a93 100644
| --- a/path0
| +++ b/path0
| @@ -17,11 +1,9 @@
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
* Move two functions into one
| #
| diff --git a/path0 b/path0
| index cd42622..495f978 100644
| --- a/path0
| +++ b/path0
| @@ -0,0 +17,11 @@
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
* Change some lines of path0
| #
| diff --git a/path0 b/path0
| index f5e09df..cd42622 100644
| --- a/path0
| +++ b/path0
| @@ -2,5 +2,5 @@
|  {
|QQ	int a = 0;
|QQ	int b = 1;
|QQ	int c;
|QQ	c = a + b;
| @@ -11,2 +11,5 @@
| +	int d = 3;
| +	int e = 5;
|QQ	printf("hello world");
| +	printf("bye!");
|  }
|  #
* Base commit
  #
  diff --git a/path0 b/path0
  new file mode 100644
  index 0000000..f5e09df
  --- /dev/null
  +++ b/path0
  @@ -0,0 +2,5 @@
  +{
  +	int a = 0;
  +	int b = 1;
  +	int c;
  +	c = a + b;
  @@ -0,0 +11,2 @@
  +	printf("hello world");
  +}
EOF

test_expect_success 'Show the line level log of path0' '
	git log --pretty=format:%s%n%b --graph -L /comb/,/^}/:path0 > current-no-M
'

test_expect_success 'validate the path0 output.' '
	test_cmp current-no-M expected-no-M
'

test_expect_success 'Show the line level log of path0 with -M' '
	git log --pretty=format:%s%n%b --graph -M -L /comb/,/^}/:path0 > current-M
'

test_expect_success 'validate the path1 output.' '
	test_cmp current-M expected-M
'

test_done
