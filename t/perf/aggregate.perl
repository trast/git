#!/usr/bin/perl

use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/../../perl/blib/lib",
	"$FindBin::Bin/../../perl/blib/arch/auto/Git";
use Git;

my $any_sign_printed = 0;

sub get_times {
	my $name = shift;
	my $firstset = shift;
	my $sig = "";
	open my $fh, "<", $name or return undef;
	my $sum_rt = 0.0;
	my $sum_u = 0.0;
	my $sum_s = 0.0;
	my $n = 0;
	while (<$fh>) {
		/^(\d+(?:\.\d+)?) (\d+(?:\.\d+)?) (\d+(?:\.\d+)?)$/
			or die "bad input line: $_";
		$sum_rt += $1;
		$sum_u += $2;
		$sum_s += $3;
		$n++;
	}
	return undef if !$n;
	close $fh or die "cannot close $name: $!";
	if (defined $firstset &&
	    open my $ph, "-|", "./t_test_score.sh $name $firstset 2>/dev/null") {
		my $result = <$ph>;
		close $ph or die "cannot close pipe to t_test_score.sh: $!";
		chomp $result;
		$sig = $result;
		if ($sig ne "") {
			$any_sign_printed = 1;
		}
	}
	return ($sum_rt/$n, $sum_u/$n, $sum_s/$n, $sig);
}

sub format_times {
	my ($r, $u, $s, $sign, $firstr) = @_;
	if (!defined $r) {
		return "<missing>";
	}
	my $out = sprintf "%.2f(%.2f+%.2f)", $r, $u, $s;
	if (defined $firstr) {
		if ($firstr > 0) {
			$out .= sprintf " %+.1f%%", 100.0*($r-$firstr)/$firstr;
		} elsif ($r == 0) {
			$out .= " =";
		} else {
			$out .= " +inf";
		}
		$out .= $sign;
	}
	return $out;
}

my (@dirs, %dirnames, %dirabbrevs, %prefixes, @tests);
while (scalar @ARGV) {
	my $arg = $ARGV[0];
	my $dir;
	last if -f $arg or $arg eq "--";
	$arg =~ s/^\^// if (! -d $arg);
	if (! -d $arg) {
		my $rev = Git::command_oneline(qw(rev-parse --verify), $arg."^{commit}");
		$dir = "build/".$rev;
	} else {
		$arg =~ s{/*$}{};
		$dir = $arg;
		$dirabbrevs{$dir} = $dir;
	}
	push @dirs, $dir;
	$dirnames{$dir} = $arg;
	my $prefix = $dir;
	$prefix =~ tr/^a-zA-Z0-9/_/c;
	$prefixes{$dir} = $prefix . '.';
	shift @ARGV;
}

if (not @dirs) {
	@dirs = ('.');
}
$dirnames{'.'} = $dirabbrevs{'.'} = "this tree";
$prefixes{'.'} = '';

shift @ARGV if scalar @ARGV and $ARGV[0] eq "--";

@tests = @ARGV;
if (not @tests) {
	@tests = glob "p????-*.sh";
}

my @subtests;
my %shorttests;
for my $t (@tests) {
	$t =~ s{(?:.*/)?(p(\d+)-[^/]+)\.sh$}{$1} or die "bad test name: $t";
	my $n = $2;
	my $fname = "test-results/$t.subtests";
	open my $fp, "<", $fname or die "cannot open $fname: $!";
	for (<$fp>) {
		chomp;
		/^(\d+)$/ or die "malformed subtest line: $_";
		push @subtests, "$t.$1";
		$shorttests{"$t.$1"} = "$n.$1";
	}
	close $fp or die "cannot close $fname: $!";
}

sub read_descr {
	my $name = shift;
	open my $fh, "<", $name or return "<error reading description>";
	my $line = <$fh>;
	close $fh or die "cannot close $name";
	chomp $line;
	return $line;
}

my %descrs;
my $descrlen = 4; # "Test"
for my $t (@subtests) {
	$descrs{$t} = $shorttests{$t}.": ".read_descr("test-results/$t.descr");
	$descrlen = length $descrs{$t} if length $descrs{$t}>$descrlen;
}

sub have_duplicate {
	my %seen;
	for (@_) {
		return 1 if exists $seen{$_};
		$seen{$_} = 1;
	}
	return 0;
}
sub have_slash {
	for (@_) {
		return 1 if m{/};
	}
	return 0;
}

my %newdirabbrevs = %dirabbrevs;
while (!have_duplicate(values %newdirabbrevs)) {
	%dirabbrevs = %newdirabbrevs;
	last if !have_slash(values %dirabbrevs);
	%newdirabbrevs = %dirabbrevs;
	for (values %newdirabbrevs) {
		s{^[^/]*/}{};
	}
}

my %times;
my @colwidth = ((0)x@dirs);
for my $i (0..$#dirs) {
	my $d = $dirs[$i];
	my $w = length (exists $dirabbrevs{$d} ? $dirabbrevs{$d} : $dirnames{$d});
	$colwidth[$i] = $w if $w > $colwidth[$i];
}
for my $t (@subtests) {
	my $firstr;
	my $firstset;
	for my $i (0..$#dirs) {
		my $d = $dirs[$i];
		$times{$prefixes{$d}.$t} = [get_times("test-results/$prefixes{$d}$t.times", $firstset)];
		my ($r,$u,$s,$sign) = @{$times{$prefixes{$d}.$t}};
		my $w = length format_times($r,$u,$s,$sign,$firstr);
		$colwidth[$i] = $w if $w > $colwidth[$i];
		if (!defined $firstr) {
			$firstr = $r;
			$firstset = "test-results/$prefixes{$d}$t.times";
		}
	}
}
my $totalwidth = 3*@dirs+$descrlen;
$totalwidth += $_ for (@colwidth);

printf "%-${descrlen}s", "Test";
for my $i (0..$#dirs) {
	my $d = $dirs[$i];
	printf "   %-$colwidth[$i]s", (exists $dirabbrevs{$d} ? $dirabbrevs{$d} : $dirnames{$d});
}
print "\n";
print "-"x$totalwidth, "\n";
for my $t (@subtests) {
	printf "%-${descrlen}s", $descrs{$t};
	my $firstr;
	for my $i (0..$#dirs) {
		my $d = $dirs[$i];
		my ($r,$u,$s,$sign) = @{$times{$prefixes{$d}.$t}};
		printf "   %-$colwidth[$i]s", format_times($r,$u,$s,$sign,$firstr);
		$firstr = $r unless defined $firstr;
	}
	print "\n";
}

if ($any_sign_printed) {
	print "-"x$totalwidth, "\n";
	print "Significance hints:  '.' 0.1  '*' 0.05  '**' 0.01  '***' 0.001\n"
}

