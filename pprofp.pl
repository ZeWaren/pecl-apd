#!/usr/bin/env perl

use Getopt::Std;

getopts('ahltTO:vz', \%opt);

if(exists $opt{'h'}) {
    usage();
}
$opt{'O'} ||= 15;

my $datafile = shift;
open DATA, $datafile;

my $index_cur;
my @callstack = (); # array of array refs of form id, utime, stime, rtime

sub parse_info($) {
        $headerName = shift;
        while(<DATA>) {
        chomp;
        last if /^END_$headerName$/;
        if (/(\w+)=(.*)/) {
            $cfg{$1} = $2;
        }
    }
    if( !exists $cfg{'hz'} ) {
        die "Incorrect header info (no clock rate";
    }
}

sub usage {
    print "
pprofp <flags> <trace file>
    Sort options
    -a          Sort by alphabetic names of subroutines.
    -l          Sort by number of calls to subroutines
    -z          Sort by user+system time spent in subroutines. (default)
    -v          Sort by average amount of time spent in subroutines.

    Display options
    -O <cnt>    Specifies maximum number of subroutines to display. (default 15)
    -t          Display compressed call tree.
    -T          Display uncompressed call tree.
";
    exit();
}    


parse_info('HEADER');
while(<DATA>) {
    chomp;
    last if /^END_TRACE$/;
    $indent_last = $indent_cur;
    $index_last = $index_cur;
    ($token, $data) = split(/ /,$_, 2);
    if ($token eq '&') {
        my ($index, $name) = split(/ /, $data, 2);
        $symbol_hash{$index} = $name;
        next;
    }
    if ($token eq '+') {
        my ($index, $notes) = split(/ /, $data, 2);
        $index_cur = $index;
        $utimes->{$index_cur}   ||= 0;
        $stimes->{$index_cur}   ||= 0;
        $rtimes->{$index_cur}   ||= 0;
        $c_utimes->{$index_cur} ||= 0;
        $c_stimes->{$index_cur} ||= 0;
        $c_rtimes->{$index_cur} ||= 0;
        $calls->{$index_cur}++;
        push @callstack,  $index_cur;
        if($opt{T}) {
            print "   "x $indent_cur . "$symbol_hash{$index_cur}\n";
            $indent_cur++;
        }
        elsif($opt{t}) {
            if ( $indent_last == $indent_cur && $index_last == $index_cur ) {
                $repcnt++;
            }
            else {
                $repstr = ' ('.++$repcnt.'x)' if $repcnt;
                print ' ' x $indent_last.$symbol_hash{$index_last}.$repstr."\n";
                $repstr = '';
                $repcnt = 0;
                $index_last = $index_cur;
                $indent_last = $indent_cur;
            }
	    $indent_cur++;
        }
        next;
    }
    if ($token eq '@') {
        my ($t_utime, $t_stime, $t_rtime) = split(/ /, $data);
        $utimes->{$index_cur} += $t_utime;
        $utotal += $t_utime;
        $stimes->{$index_cur} += $t_stime;
        $stotal += $t_stime;
        $rtimes->{$index_cur} += $t_rtime;
        $rtotal += $t_rtime;
        foreach $stack_element (@callstack) {
            $c_utimes->{$stack_element} += $t_utime;
            $c_stimes->{$stack_element} += $t_stime;
            $c_rtimes->{$stack_element} += $t_rtime;
        }
        next;
    }
    if ($token eq '-') {
        my ($index, $notes) = split(/ /, $data, 2);
        $indent_cur--;
        pop @callstack;
        next;
   }
}
parse_info('FOOTER');

print "\n\n
         Real	      User        System           secs/    cumm
%Time (excl/cumm)  (excl/cumm)  (excl/cumm) Calls  call    s/call Name
----------------------------------------------------------------------\n";

sub by_time {
	($stimes->{$b} + $utimes->{$b}) <=> ($stimes->{$a} + $utimes->{$a});
}

sub by_avgcpu {   
        ($stimes->{$b} + $utimes->{$b})/$calls->{$b} <=> ($stimes->{$a} + $utimes->{$a})/$calls->{$a};
}

sub by_calls {
	$calls->{$b} <=> $calls->{$a};
}

sub by_name {
	uc($symbol_hash{$a}) cmp uc($symbol_hash{$b});
}

my $sort = 'by_time';
$sort = 'by_calls' if exists $opt{l};
$sort = 'by_name' if exists $opt{a};
$sort = 'by_avgcpu' if exists $opt{v};


my $l = 0;
foreach $j (sort $sort  keys %symbol_hash) {
    last if $l++ > $opt{'O'};
    $pcnt = ($stimes->{$j} + $utimes->{$j})/($utotal + $stotal + $itotal);
    $c_pcnt = ($c_stimes->{$j} + $utimes->{$j})/($utotal + $stotal + $itotal);
    $rsecs = $rtimes->{$j};
    $rsecs /= $cfg{'hz'};
    $c_rsecs = $c_rtimes->{$j};
    $c_rsecs /= $cfg{'hz'};
    $usecs = $utimes->{$j};
    $usecs /= $cfg{'hz'};
    $c_usecs = $c_utimes->{$j};
    $c_usecs /= $cfg{'hz'};
    $ssecs = $stimes->{$j};
    $ssecs /= $cfg{'hz'};
    $c_ssecs = $c_stimes->{$j};
    $c_ssecs /= $cfg{'hz'};
    $ncalls = $calls->{$j};
    $percall =  $ncalls?($usecs + $ssecs)/$ncalls:0;
    $cpercall = $ncalls?($c_usecs + $c_ssecs)/$ncalls:0;
    $name = $symbol_hash{$j};
    $~ = FULL_STAT;
    write;
}

format FULL_STAT =
^>>>  ^>>>> ^>>>> ^>>>> ^>>>> ^>>>> ^>>>>   ^>>>>  ^>>>>   ^>>>>  ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$pcnt, $rsecs, $c_rsecs, $usecs, $c_usecs, $ssecs, $c_ssecs, $ncalls, $percall, $cpercall, $name
.

