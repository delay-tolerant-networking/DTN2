#!/usr/bin/perl
use Getopt::Long;


$make_cumul = 0;
$make_bars = 0;
$outputfile = "res.txt";

GetOptions( 
          "cumul" => \$make_cumul,
          "bars" => \$make_bars,
          "o=s" => \$outputfile,
          );

$run_no = 2;
$base="./logs/run$run_no/";


if($run_no == 2)
{
    @loss_rates=("L0","L0.01","L0.02","L0.05");
    @loss_rates=("L0","L0.02");
    @sizes=("1","10","20","40","60","100","1000");
    @nums=("500","200","100","50","50","50","10");
    $is_rabin = 1;
}
elsif($run_no == 1)
{
    @loss_rates=("L0");
    @sizes=("1","10","100");
    @nums=("500","200","50");
    $is_rabin = 1;
}
%protos=("tcp0" => "TCP (E2E)",
         "tcp1" => "TCP (HOP)",
         "dtn0" => "DTN (E2E)",
         "dtn1" => "DTN (HOP)",
         "mail0" => "MAIL (E2E)",
         "mail1" => "MAIL (HOP)"
         );


$bandwidth = "100";
$nodes=5;

if($make_cumul == 1)
{
    foreach $loss (@loss_rates)
    {
        $index=0;
        foreach $size (@sizes)
        {
            $num=$nums[$index];
            if($is_rabin == 1)
            {
                $key="rabin$size-N$nodes-$loss-M$num-S$size-B$bandwidth";
                $command="perl makeplot.pl -base $base/$loss/ -exp $key";
                print "Executing : $command ....\n";
                system($command);
            }
            $index=$index+1;
        }
    }
}





if($make_bars == 1)
{
    open(FOUT,">$outputfile") || die ("cannot open file:$outputfile\n");
    

    $topline = "";
    $datalines = "";
    foreach $loss (@loss_rates)
    {
        $index=0;
        foreach $size (@sizes)
        {
            $num=$nums[$index];
            if($is_rabin == 1)
            {
                $key="rabin$size-N$nodes-$loss-M$num-S$size-B$bandwidth";
                
                $printline = "$size KB";
                $topline = "XX";
                foreach $proto(keys %protos)
                {
                    $fname = "$base/$loss/$key/$proto/times.txt";
                    $firstline = &get_first_line($fname); 
                    print "Fname: $fname \n\tAnd line:$firstline";
                    @parts=split(/[ \t]+/,$firstline);
                    
                    $total_sent=$size*$parts[5];
                    $total_time=$parts[4]-$parts[1];
                    $used_bw = 8.0*$total_sent/$total_time;
                    $fraction= $used_bw/$bandwidth;
                    print "\t[$fraction] --> sent:$total_sent time:$total_time used:$used_bw \n";
                    
                    $printline = "$printline,$fraction";
                    $topline = "$topline,$protos{$proto}";
                    print "\n";
                }
                $datalines="$datalines $printline\n";
            }
            $index=$index+1;
        }
    }
    print FOUT "$topline\n";
    print FOUT $datalines;  
    close(FOUT); 
}

exit;


sub get_first_line() {
    my($fname) = @_;
    open(F,"$fname") || die ("cannot open file :$fname\n");
    $line = <F>;
    close(F);
    return $line;
}


