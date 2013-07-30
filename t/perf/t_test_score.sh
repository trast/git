#!/bin/sh

# If the user doesn't have R, we don't care

command -v R >/dev/null || exit 0

# Uses R to run a t-test on the hypothesis that the elapsed time
# values in $1 are less than the ones in $2.

pvalue=$(R --no-save --slave <<-EOF
	a <- read.table("$1")
	b <- read.table("$2")
	tst <- t.test(a\$V1, b\$V1)
	p <- tst\$p.value
	if (p<0.001) print ("***") \
	else if (p<0.01) print ("**") \
	else if (p<0.05) print ("*") \
	else if (p<0.1) print (".")
EOF
)

pvalue=${pvalue##\[1\] \"}
pvalue=${pvalue%%\"}
echo "$pvalue"
