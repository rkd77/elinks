#! /usr/bin/perl
use strict;
use warnings;
use Locale::PO qw();

my %contexts;
my($pofile) = @ARGV;
my $pos = do {
    # Locale::PO 0.16 doesn't understand "#~" obsolete lines and spews warnings.
    local $SIG{__WARN__} = sub {};
    Locale::PO->load_file_asarray($pofile) or die "$pofile: $!";
};
foreach my $po (@$pos) {
    next if $po->fuzzy();
    my $msgstr = $po->msgstr()
	or next;
    my($accelerator) = ($msgstr =~ /~(.)/)
	or next;
    $accelerator = uc($accelerator);
    my $automatic = $po->automatic()
	or next;
    my($contexts) = ($automatic =~ /^accelerator_context\(([^\)]*)\)/)
	or next;
    foreach my $context (split(/\s*,\s*/, $contexts)) {
	my $prev = $contexts{$context}{$accelerator};
	if (defined($prev)) {
	    warn "$pofile: Accelerator conflict for \"$accelerator\" in \"$context\":\n";
	    warn "$pofile:  1st msgid " . $prev->msgid() . "\n";
	    warn "$pofile:  1st msgstr " . $prev->msgstr() . "\n";
	    warn "$pofile:  2nd msgid " . $po->msgid() . "\n";
	    warn "$pofile:  2nd msgstr " . $po->msgstr() . "\n";
	} else {
	    $contexts{$context}{$accelerator} = $po;
	}
    }
}
