#!/usr/bin/perl
use Getopt::Long;

my $logscalex = 0;
my $logscaley = 0;


my $expname='';
my $base='';


my $nodes=4;
my $loss=0;
my $bandwidth=100;
my $size=10;
my $num=100;


GetOptions( 
          "exp=s" => \$expname,
          "base=s" => \$base,
          "nodes=i" => \$nodes,
          "num=i" => \$num,
          );

if($base eq '')
{
    print "base not given\n";
    exit;
}
if($expname eq '')
{
    print "expname not given\n";
    exit;
}
$expbase="$base/$expname";


if ( !(-d $expbase))
{
    print "$expbase does not exist\n";
    exit;
}

%protos=("tcp0" => "TCP (E2E)",
         "tcp1" => "TCP (HOP)",
         "dtn0" => "DTN (E2E)",
         "dtn1" => "DTN (HOP)",
         "mail0" => "MAIL (E2E)",
         "mail1" => "MAIL (HOP)"
         );

$plotbase="$expbase/gnuplot";
$logbase="$plotbase/log";
$psbase="$plotbase/ps";

mkdir($plotbase);
mkdir($logbase);
mkdir($psbase);

if($expname =~ /rabin/g)
{
    print "Getting info about experiment ..\n";
    $expname =~ /([\w]+)-N([1-9])-L([0-9\.]+)-M([0-9]+)-S([0-9]+)-B([0-9]+).*/g;
    $nodes=$2;
    $loss=$3;
    $num=$4;
    $size=$5;
    $bandwidth=$6;
}
print "Name: Nodes:$nodes Loss:$loss Num:$num Size:$size Bandwidth:$bandwidth\n";

$command="maketable.sh $nodes $expname $base";
print "First generating the time table ... : $command\n";
system($command);


### the code below needs a  $key ...$costo

$vacfile="$plotbase/plot.gnp";

print "\n\nNow starting gnuplot generation stuff ...\n";

open (VACFILE, ">$vacfile") || die ("cannot open:$vacfile\n");


#p1("");
&p1("set pointsize 2.0 ;
set linestyle 1 lt 7 lw 2.5 pt 7 ps 2 ;
set linestyle 2 lt 12 lw 2.5 pt 2 ps 2 ;
set linestyle 3 lt 3 lw 2.5 pt 3 ps 2 ;
set linestyle 4 lt 5  lw 2.5 pt 4 ps 2 ;
set linestyle 5 lt 1 lw 2.5 pt 1 ps 2 ;
set linestyle 6 lt 8 lw 2.5 pt 5 ps 2 ;
set linestyle 7 lt 15 lw 2.5 pt 17 ps 2 ;
set linestyle 8 lt 3 lw 2.5 pt 15 ps 2 ; 
");


&p1("set ylabel ", &qs("Message index"));
&p1("set xlabel ", &qs("Delivery time(s)"));


$gx_init = $logscalex;
$gy_init = $logscaley;


#&p1("set xrange [$gx_init:1.1]");
#&p1("set yrange [$gy_init:1.1]");
&p1("!touch empty ; \n"); 
&p1("set xrange [$gx_init:500]");
&p1("set yrange [$gx_init:500]");
&p1("set term x11 1 ; ","plot ",&qs("empty"), " title ",&qs(""));



$plottitle="Nodes:$nodes Size:$size Loss=$loss" ;
&p1("set title ",&qs($plottitle));

$lsid = 1;


$xrange=100;
$yrange=100;
foreach $proto (keys %protos) {
    my $timesfile="$expbase/$proto/times.txt";

    if ( !(-e $timesfile)) 
    {
        next;
    }
    
    open(FIN,"$timesfile") || die("cannot open file:$timesfile\n");
    $lineno=0;
    $startime=0;$endtime=0;
    while($line=<FIN>)
    {
        @parts=split(/[ \t]+/,$line);
        if($lineno == 0) {
            $startime = $parts[1];
        }
        $endtime=$parts[2];
        $lineno=$lineno+1;
    }
    close(FIN);
    
    $diff=$endtime-$startime;
    if($yrange < $diff) {$yrange=$diff;}
    
    print "good: $timesfile start:$startime end:$endtime yrange:$yrange\n";
    
    my $title ="$proto";

    #&p1( "replot  ",&qs($timesfile), "  using 1:(\$3-$startime) title ",&qs($title), " with lines ls $lsid");
    &p1( "replot  ",&qs($timesfile), "  using (\$3-$startime):1 title ",&qs($title), " with lines ls $lsid");
    $lsid++;
} 
    
$xrange=$num*1.2;
$yrange=$yrange*1.2;


&p1("set xrange [$gx_init:$yrange]");
&p1("set yrange [$gy_init:$xrange]");

&p1("replot");
&p1("set term postscript noenhanced  20 ; "," set output ",&qs("$psbase/plot.ps"), " ; replot ; set term x11 ; ");



close(VACFILE);
print "Run : \ngnuplot $plotbase/plot.gnp\n";
print "psfile: $psbase/plot.ps\n";




system ("gnuplot $plotbase/plot.gnp");

exit;


sub qs() {    return "\"" . $_[0] ."\""  ;}  #print &qs("HELLO ") ;
sub p1() { 
    foreach $foo (@_) {
        print VACFILE $foo;
    }
	print VACFILE "\n";
}

sub find() {
    $_key=$_[0];
    $_file =$_[1];
    open(FILE,$_file) or  die " file not found $_file";
    while(<FILE>) {
	    $foo=$_;
        #	print $foo;
	    #if (($key,$gap,$val) = ($foo =~ /(.*)($_key)(\s*=\s*)([\d:_\.]*\b)(.*)/)){
        #    return $4;
        #}
    }
    close(FILE);
}


