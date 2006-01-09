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

__END__

=head1 NAME

check-accelerator-contexts.pl - Scan a PO file for conflicting
accelerator keys.

=head1 SYNOPSIS

B<check-accelerator-contexts.pl> F<I<language>.pot>

=head1 DESCRIPTION

B<check-accelerator-contexts.pl> is part of a framework that detects
conflicting accelerator keys in Gettext PO files.  A conflict is when
two items in the same menu or two buttons in the same dialog box use
the same accelerator key.

The PO file format does not normally include any information on which
strings will be used in the same menu or dialog box.
B<check-accelerator-contexts.pl> can only be used on PO files to which
this information has been added with B<gather-accelerator-contexts.pl>
or merged with B<msgmerge>.

B<check-accelerator-contexts.pl> reads the F<I<language>.po> file
named on the command line and reports any conflicts to standard error.

B<check-accelerator-contexts.pl> does not access the source files to
which F<I<language>.po> refers.  Thus, it does not matter if the line
numbers in "#:" lines are out of date.

=head1 ARGUMENTS

=over

=item F<I<language>.po>

The PO file to be scanned for conflicts.  This file must include the
"accelerator_context" comments added by B<gather-accelerator-contexts.pl>.
If the special comments are missing, no conflicts will be found.

=back

=head1 BUGS

B<check-accelerator-contexts.pl> reports the same conflict multiple
times if it occurs in multiple contexts.

Jonas Fonseca suggested the script could propose accelerators that are
still available.  This has not been implemented.

=head2 Waiting for Locale::PO fixes

Locale::PO does not understand "#~" lines and spews warnings about
them.  There is an ugly hack to hide these warnings.

The warning messages should include line numbers, so that users of
Emacs could conveniently edit the conflicting part of the PO file.
This is not feasible with the current version of Locale::PO.

When B<check-accelerator-contexts.pl> includes C<msgstr> strings in
warnings, it should transcode them from the charset of the PO file to
the one specified by the user's locale.

=head1 AUTHOR

Kalle Olavi Niemitalo <kon@iki.fi>

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2005-2006 Kalle Olavi Niemitalo.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.  In addition:

=over

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

=back

=head1 SEE ALSO

L<gather-accelerator-contexts.pl>, C<xgettext(1)>, C<msgmerge(1)>
