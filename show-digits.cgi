#!/usr/local/bin/perl

use CGI;

$cgi = new CGI;

$cntr_handler = "/server-cntr";
#$cntr_handler = "count.cgi";

print $cgi->header;

CGI::ReadParse();

$in{face} ||= "default";

print <<"EOF";
<html>
<head><title>Sample Digit: $in{face}</title></head>
<body>
<center>
<h2>Show Digit: $in{face}</h2>
<b>Normal:</b>
<tt>&lt;img src="$cntr_handler?face=$in{face}"&gt;</tt><br>
<img src="$cntr_handler?fcount=1234567890&face=$in{face}"><br>
<p>
<b>Transparent:</b>
<tt>&lt;img src="$cntr_handler?face=$in{face}&amp;trans"&gt;</tt><br>
<img src="$cntr_handler?fcount=1234567890&face=$in{face}&trans"><br>

<hr>
<h3>Click typefaces below to try:<h3>
<center><table>
EOF

$facedir = $ENV{URL_COUNT_FACEDIR};
opendir(DIGITS, $facedir) or die("Can't open directory $digitdir!\n");

$i = 0;
foreach $face (grep(!/^\.\.?/, readdir(DIGITS))){
    print "<tr>\n" if $i % 8 == 0;
    print qq(<th><a href="$ENV{'SCRIPT_NAME'}?face=$face">$face</a></th>);
    $i++;
}

print <<"EOF";
</table>
</center></body></ht>
EOF

