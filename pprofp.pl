#!/usr/bin/env perl

use Getopt::Std;

getopts('tT', \%opt);
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

print "\n\n%Time Real(excl/cum)  User(excl/cum)  Sys(excl/cum)  Calls  s/call Cs/call Name\n";

sub by_percall {
	($stimes->{$b} + $utimes->{$b}) <=> ($stimes->{$a} + $utimes->{$a});
}

my $l = 0;
foreach $j (sort by_percall  keys %symbol_hash) {
    last if $l++ > 20;
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
^>>>     ^>>>> ^>>>>     ^>>>> ^>>>>    ^>>>> ^>>>>   ^>>>>  ^>>>>   ^>>>> ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$pcnt, $isecs, $c_isecs, $usecs, $c_usecs, $ssecs, $c_ssecs, $ncalls, $percall, $cpercall, $name
.

