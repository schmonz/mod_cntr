#!/usr/local/bin/perl

use Getopt::Std;

&getopts('c:d:r:v:s:') || &Usage;

if ($opt_v) {
    $opt_v = shift if ($opt_v == 1);
} else {
    $opt_v = shift if (@ARGV > 1);
}

($file = shift) || &Usage;
&Usage if @ARGV;

# Validate args
die "Specify only one of option at a time !!\n"
    if (($opt_c && ($opt_d . $opt_r . $opt_v . $opt_s)) ||
	($opt_d && ($opt_r . $opt_v . $opt_s)) ||
	($opt_s && ($opt_r . $opt_v)) ||
	($opt_r && $opt_v));

if ($opt_s) {
    print "Enter count for \"$opt_s\": ";
    chop($set_count = <STDIN>);
}

# URL should be in one of the options. 
# If empty, then just display all entries
$url = $opt_c . $opt_d . $opt_r . $opt_v . $opt_s;

# No directories allowed...
die "Sorry, URL may not be a directory.\n" if ($url =~ /\/$/);
die "Sorry, URL must begin with a '/'.\n" if ($url && $url !~ /^\//);

# Figure out what type of file...
# strip any DBM style extension
$file =~ s/\.(\w+)$//;
$ext = $1;

# Check for regular ascii
if (-r "$file.$ext") {
	&dbmFile($file);
} else {
	print STDERR "Cannot find either $file.$ext\n";
	exit 1;
}
exit 0;

#

# Search DBM file for URL

sub dbmFile {
    local($file) = @_;
    local($dbmfile) = "$file.$ext";

    # Not ascii, try DBM
    dbmopen(%DB, $file, 0644) || die "Unable to open DBM file: $!\n";
    open(IN, "<$dbmfile");
    fcntl(IN, 7, 0);  # Lock
    if (!$url) {
	while (($furl,$val) = each %DB) {
	    ($count, $reset) = unpack("I2", $val);
	    print "$furl\t$count\t$reset\n";
	}
    } elsif (defined $DB{"$url"}) {
	($count, $reset) = unpack("I2", $DB{"$url"});
	if ($opt_v) {
	    print "$url\t$count\t$reset\n";
	} elsif ($opt_d) {
	    delete $DB{"$url"};
	} elsif ($opt_r) {
	    $count = 0; $reset = time();
	    $DB{"$url"} = pack("I2", $count, $reset);
	} elsif ($opt_s) {
	    $count = $set_count; $reset = time();
	    $DB{"$url"} = pack("I2", $count, $reset);
	}
    } elsif ($opt_c) {
	$count = 0; $reset = time();
	$DB{"$url"} = pack("I2", $count, $reset);
    } else {
	print "URL $url not found\n";
    }
    fcntl(IN, 3, 0);  # Unlock
    close(IN);
    dbmclose(%DB);
}

sub Usage {
    print STDERR <<"EOF";
Usage: $0 [ -cdrsv URL ] counter_log
       -c     create URL
       -d     delete URL
       -r     reset counter
       -s     set counter
       -v     view counter

     Create has no effect if URL exists, delete and 
     reset have no effect if URL doesn't exist.
     View will display counter and reset date.
     If no option is supplied, then file is dumped
     displaying all entries.
EOF
	exit 1;
}
