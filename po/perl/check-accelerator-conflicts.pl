#! /usr/bin/perl
# The copyright notice and license are in the POD at the bottom.

use strict;
use warnings;
use Locale::PO qw();
use Getopt::Long qw(GetOptions :config bundling gnu_compat);
use autouse 'Pod::Usage' => qw(pod2usage);

my $VERSION = "1.3";

sub show_version
{
    print "check-accelerator-conflicts.pl $VERSION\n";
    pod2usage({-verbose => 99, -sections => "COPYRIGHT AND LICENSE",
	       -exitval => 0});
}

my $Accelerator_tag;

sub po_arrays_eq ($$)
{
    my($left, $right) = @_;
    ref($left) eq "ARRAY" or return 0;
    ref($right) eq "ARRAY" or return 0;
    @$left == @$right or return 0;
    $left->[$_] eq $right->[$_] or return 0
	foreach (0 .. $#$right);
    return 1;
}

sub check_po_file ($)
{
    my($po_file_name) = @_;
    my %accelerators;
    my $warnings = 0;

    {
	my $pos = Locale::PO->load_file_asarray($po_file_name)
	    or warn "$po_file_name: $!\n", return 2;
	foreach my $po (@$pos) {
	    next if $po->fuzzy();
	    my $msgstr = $po->msgstr()
		or next;
	    my($accelerator) = ($msgstr =~ /\Q$Accelerator_tag\E(.)/s)
		or next;
	    $accelerator = uc($accelerator);
	    my $automatic = $po->automatic()
		or next;
	    my($contexts) = ($automatic =~ /^accelerator_context\(([^\)]*)\)/)
		or next;
	    foreach my $context (split(/\s*,\s*/, $contexts)) {
		push @{$accelerators{$accelerator}{$context}}, $po;
	    }
	}
    }

    foreach my $accelerator (sort keys %accelerators) {
	my $ctxhash = $accelerators{$accelerator};
	foreach my $outer_ctxname (sort keys %$ctxhash) {
	    # Cannot use "foreach my $pos" directly, because $pos
	    # would then become an alias and change to 0 below.
	    my $pos = $ctxhash->{$outer_ctxname};
	    if (ref($pos) eq "ARRAY" && @$pos > 1) {
		my @ctxnames_in_conflict;
		foreach my $ctxname (sort keys %$ctxhash) {
		    if (po_arrays_eq($ctxhash->{$ctxname}, $pos)) {
			push @ctxnames_in_conflict, $ctxname;
			$ctxhash->{$ctxname} = 0;
		    }
		}
		my $ctxnames_in_conflict = join(", ", map(qq("$_"), @ctxnames_in_conflict));
		warn "$po_file_name: Accelerator conflict for \"$accelerator\" in $ctxnames_in_conflict:\n";
		foreach my $po (@$pos) {
		    my $lineno = $po->msgstr_begin_lineno();
		    my $msgstr = $po->msgstr();

		    # Get a string of unique characters in $msgstr,
		    # preferring characters that start a word.
		    my $displaystr = $msgstr;
		    $displaystr =~ s/\Q$Accelerator_tag\E//g;
		    my $suggestions = "";
		    foreach my $char ($displaystr =~ /\b(\w)/g,
				      $displaystr =~ /(\w)/g) {
			$suggestions .= $char unless $suggestions =~ /\Q$char\E/i;
		    }

		    # But don't suggest unavailable characters.
		    SUGGESTION: foreach my $char (split(//, $suggestions)) {
			foreach my $ctxname (@ctxnames_in_conflict) {
			    $suggestions =~ s/\Q$char\E//, next SUGGESTION
				if exists $accelerators{uc($char)}{$ctxname};
			}
		    }

		    warn "$po_file_name:$lineno: msgstr $msgstr\n";
		    if ($suggestions eq "") {
			warn "$po_file_name:$lineno: no suggestions :-(\n";
		    }
		    else {
			warn "$po_file_name:$lineno: suggestions: $suggestions\n";
		    }
		}
		$warnings++;
	    } # if found a conflict 
	} # foreach context known for $accelerator
    } # foreach $accelerator
    return $warnings ? 1 : 0;
}

GetOptions("accelerator-tag=s" => sub {
	       my($option, $value) = @_;
	       die "Cannot use multiple --accelerator-tag options\n"
		   if defined($Accelerator_tag);
	       die "--accelerator-tag requires a single-character argument\n"
		   if length($value) != 1;
	       $Accelerator_tag = $value;
	   },
	   "help" => sub { pod2usage({-verbose => 1, -exitval => 0}) },
	   "version" => \&show_version)
    or exit 2;
$Accelerator_tag = "~" unless defined $Accelerator_tag;
print(STDERR "$0: missing file operand\n"), exit 2 unless @ARGV;

my $max_error = 0;
foreach my $po_file_name (@ARGV) {
    my $error = check_po_file($po_file_name);
    $max_error = $error if $error > $max_error;
}
exit $max_error;

__END__

=head1 NAME

check-accelerator-conflicts.pl - Scan a PO file for conflicting
accelerator keys.

=head1 SYNOPSIS

B<check-accelerator-conflicts.pl> [I<option> ...] F<I<language>.po> [...]

=head1 DESCRIPTION

B<check-accelerator-conflicts.pl> is part of a framework that detects
conflicting accelerator keys in Gettext PO files.  A conflict is when
two items in the same menu or two buttons in the same dialog box use
the same accelerator key.

The PO file format does not normally include any information on which
strings will be used in the same menu or dialog box.
B<check-accelerator-conflicts.pl> can only be used on PO files to which
this information has been added with B<gather-accelerator-contexts.pl>
or merged with B<msgmerge>.

B<check-accelerator-conflicts.pl> reads the F<I<language>.po> file
named on the command line and reports any conflicts to standard error.

B<check-accelerator-conflicts.pl> does not access the source files to
which F<I<language>.po> refers.  Thus, it does not matter if the line
numbers in "#:" lines are out of date.

=head1 OPTIONS

=over

=item B<--accelerator-tag=>I<character>

Specify the character that marks accelerators in C<msgstr> strings.
Whenever this character occurs in a C<msgstr>,
B<check-accelerator-conflicts.pl> treats the next character as an
accelerator and checks that it is unique in each of the contexts in
which the C<msgstr> is used.

Omitting the B<--accelerator-tag> option implies
B<--accelerator-tag="~">.  The option must be given to each program
separately because there is no standard way to save this information
in the PO file.

=back

=head1 ARGUMENTS

=over

=item F<I<language>.po> [...]

The PO files to be scanned for conflicts.  These files must include the
"accelerator_context" comments added by B<gather-accelerator-contexts.pl>.
If the special comments are missing, no conflicts will be found.

=back

=head1 EXIT CODE

0 if no conflicts were found.

1 if some conflicts were found.

2 if the command line is invalid or a file cannot be read.

=head1 BUGS

=head2 Waiting for Locale::PO fixes

When B<check-accelerator-conflicts.pl> includes C<msgstr> strings in
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
