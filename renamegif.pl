#!/usr/local/bin/perl
use Getopt::Std;
use File::Basename;

getopts("h");

&help if $opt_h;

for (@ARGV){
    die "Wrong format: $_" unless m,(\d),io;
    $d = dirname($_);
    $f = "$1.gif";
    print "$_ ->$d/$f\n";
    rename $_, "$d/$f" or die "$_:$!";
}

sub help{
    my $name = $0; $name =~ s,.*/,,;
    print <<"EOF";
$name <GIF files to rename>
    -h shows this message;
EOF
}
