use strict;
use warnings;
use diagnostics;

if (scalar(@ARGV) != 1){
	print "Usage: perltests.pl <directory>\n";
	exit;
}

my $dir = $ARGV[0];


#long file name
print "Test long filename\n";
my $fname = "";
foreach (1..255) {
    $fname .="l";
}
`touch $dir/$fname`;
doWait();

#test soft links
print "Testing soft links\n";
`rm -f $dir/b`;
`touch $dir/a`;
`ln -s $dir/a $dir/b`;
`echo "content" >> $dir/b`;
doWait();


#test large numbers of files
print "Touching 300 files...\n";
for my $i (1..300){
	`touch $dir/file$i`;
}

doWait();
print "Removing files...\n";
`rm -rf $dir/*`;




sub doWait {
	print "Press enter to continue...\n";
	<STDIN>;
}

