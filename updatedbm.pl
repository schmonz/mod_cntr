#!/usr/local/bin/perl

use Time::Local;
BEGIN{
    @AnyDBM_File::ISA = qw(DB_File NDBM_File GDBM_File SDBM_File ODBM_File);
    use AnyDBM_File;
    @mon2mname = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
    for ($i = 0; $i <= $#mon2mname; $i++){ $mname2mon{$mon2mname[$i]} = $i;};
}


for (@ARGV){
    $_ =~ s/\.(\w+)$//o;
    $ext = $1;
    rename "$_.$ext", "$_.old.$ext";
    if (! dbmopen(%old, "$_.old", 0444)){
	die $!;
    }else{
	die $! unless dbmopen(%new, $_, 0666)
    }
    for $k (sort keys %old){
	die "$_.$ext is in new format already!" if length($old{$k}) == 8;
	# 1    Monday, 28-Sep-98 01:31:53 JST
	print  "$k:$old{$k}\n";
	($count, $day, $mday, $mname, $year, $hh, $mm, $ss)
	    = split(/[\s,\-\:]+/, $old{$k});
	# print join(",", ($count, $day, $mday, $mname, $year, $hh, $mm, $ss)), "\n";
	$reset = timelocal($ss,$mm,$hh,$mday,$mname2mon{$mname},$year);
	$new{$k} = pack("I2", $count, $reset);
    }
    dbmclose(%new);
    dbmclose(%old);
    chmod 0666, "$_.$ext";
}

