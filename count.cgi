#!/usr/local/bin/perl
#
# $Id$
#

use 5.004;
use strict;

#
# config
#
use vars qw($FACEDIR $DEFAULT_FACE $ENV %ENV);
$FACEDIR = $ENV{URL_COUNT_FACEDIR} || "/usr/local/apache/share/digits";
$DEFAULT_FACE = "default";

#
# init
#

use vars qw(%in $in);

use CGI;
CGI::ReadParse(*in);

my $facedir = ($in{face} and -d "$FACEDIR/$in{face}") ?
    "$FACEDIR/$in{face}" : "$FACEDIR/$DEFAULT_FACE";
my $count = $in{count} || getCount($in{url});
my $strformat = $in{ndigit} =~ /^\d+$/ ? "%0$in{ndigit}d" : "%d";
my $dstr = sprintf($strformat, $count);
my $cgi = new CGI;
my $trans = $in{trans} ? 1 : 0;
my $image = mkImage($dstr, $facedir, $trans);

#
# Action
#

print $cgi->header("-type"    => "image/gif",
             "-length"  => length($image),
             "-Expires" => "Thursday, 01-Jan-1970 00:00:00",
             "-Pragma"  => "No-Cache");

print $image;

#
# subs
#

sub getCount($){
    my $dbmfile = $ENV{URL_COUNT_DB};
    return 0 unless $dbmfile;
    my $url = shift;
    unless ($url){
        use URI::URL;
        $url = url($ENV{HTTP_REFERER})->epath;
        return 0 unless $url;
    }
    my ($dindex, $newurl);

    my (%dbm);
    my ($count, $reset) = (0,0);
    
    dbmopen(%dbm, $dbmfile, 0444) or return 0;

    $newurl = $url;

    unless (defined $dbm{$newurl}){
        for $dindex (split(/\s+/, $ENV{URL_COUNT_DINDEX})){
            $newurl = $url . $dindex;
            last if defined $dbm{$newurl};
        }
    }
    if (defined $dbm{$newurl}){
	($count, $reset) = unpack("I2", $dbm{$newurl});
    }
    dbmclose(%dbm);
    return $count;
}

#
# The following needs GD.pm
#

use GD;

sub mkImage($$$){
    my ($dstr, $facedir, $trans) = @_;
    my ($sx, $sy, $dx, $dy);
    my ($i, %dimage);

    # read needed face files;

    for ($i = 0; $i < length($dstr); $i++) {
        my $c = substr($dstr, $i, 1);
        unless (defined $dimage{$c}){
            $dimage{$c} = readImage($c, $facedir);
        }
        ($dx, $dy) = $dimage{$c}->getBounds();
        $sx += $dx;
        $sy = $dy if $dy > $sy;
    }

    my $result = GD::Image->new($sx, $sy);
    $sx = 0;

    # now render;

    for ($i = 0; $i < length($dstr); $i++) {
        my $c = substr($dstr, $i, 1);
        ($dx, $dy) = $dimage{$c}->getBounds();
        $result->copy($dimage{$c}, $sx, 0, 0, 0, $dx, $dy);
        $sx += $dx;
    }
    $result->transparent($result->getPixel(0,0)) if $trans;
    $result->interlaced('true');
    return $result->gif();
}

sub readImage($$){
    my ($c, $facedir) = @_;
    my $giffile = "$facedir/$c.gif";
    no strict 'subs'; # otherwise filehandle will kill the script!
    open GIF, $giffile or die "$giffile:$!";
    my $result = newFromGif GD::Image(GIF) || die "$giffile:$!";
    close GIF;
    return $result;
}
