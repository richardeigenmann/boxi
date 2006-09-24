#!/usr/bin/perl

use strict;
use warnings;

use lib 'infrastructure';
use BoxPlatform;

my ($test_name,$test_mode) = @ARGV;

$test_mode = 'debug' if not defined $test_mode or $test_mode eq '';

if($test_name eq '' || ($test_mode ne 'debug' && $test_mode ne 'release'))
{
	print <<__E;
Run Test utility -- bad usage.

runtest.pl (test|ALL) [release|debug]

Mode defaults to debug.

__E
	exit(2);
}

my @results;
my $exit_code = 0;

if($test_name ne 'ALL')
{
	# run one or more specified test
	if ($test_name =~ m/,/)
	{
		foreach my $test (split m/,/, $test_name)
		{
			runtest($test);
		}
	}
	else
	{
		runtest($test_name);
	}
}
else
{
	# run all tests
	my @tests;
	open MODULES,'modules.txt' or die "Can't open modules file";
	while(<MODULES>)
	{
		# omit bits on some platforms?
		next if m/\AEND-OMIT/;
		if(m/\AOMIT:(.+)/)
		{
			if($1 eq $build_os or $1 eq $target_os)
			{
				while(<MODULES>)
				{
					last if m/\AEND-OMIT/;	
				}
			}
			next;
		}
		push @tests,$1 if m~\Atest/(\w+)\s~;
	}
	close MODULES;
	
	runtest($_) for(@tests)
}

# report results
print "--------\n",join("\n",@results),"\n";

exit $exit_code;

sub runtest
{
	my ($t) = @_;

	# attempt to make this test
	my $flag = ($test_mode eq 'release')?(BoxPlatform::make_flag('RELEASE')):'';
	my $make_res = system("cd test/$t ; $make_command $flag");
	if($make_res != 0)
	{
		push @results,"$t: make failed";
		$exit_code = 2;
		return;
	}
	
	# run it
	my $test_res = system("cd $test_mode/test/$t ; ./t | tee ../../../temp.runtest");

	# open test results
	if(open RESULTS,'temp.runtest')
	{
		my $last;
		while(<RESULTS>)
		{
			$last = $_ if m/\w/;
		}
		close RESULTS;

		chomp $last;
		push @results,"$t: $last";

		if ($last ne "PASSED") 
		{ 
			$exit_code = 1;
		}
	}
	else
	{
		push @results,"$t: output not found";
	}
	
	# delete test results
	unlink 'temp.runtest';
}

