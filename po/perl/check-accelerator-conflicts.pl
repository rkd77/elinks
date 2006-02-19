#! /usr/bin/perl
# The copyright notice and license are in the POD at the bottom.

use strict;
use warnings;
use Locale::PO qw();
use Getopt::Long qw(GetOptions :config bundling gnu_compat);
use autouse 'Pod::Usage' => qw(pod2usage);

my $VERSION = "1.4";

sub show_version
{
    print "check-accelerator-conflicts.pl $VERSION\n";
    pod2usage({-verbose => 99, -sections => "COPYRIGHT AND LICENSE",
	       -exitval => 0});
}

# The character that precedes accelerators in strings.
# Set with the --accelerator-tag=CHARACTER option.
my $Opt_accelerator_tag;

# True if, for missing or fuzzy translations, the msgid string should
# be checked instead of msgstr.  Set with the --msgid-fallback option.
my $Opt_msgid_fallback;

sub acceleration_arrays_eq ($$)
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
	    my $automatic = $po->automatic()
		or next;
	    my($ctxnames) = ($automatic =~ /^accelerator_context\(([^\)]*)\)/)
		or next;
	    my @ctxnames = split(/\s*,\s*/, $ctxnames);
	    my @accelerations;
	    my $msgid = $po->msgid();
	    my $msgstr = $po->msgstr();
	    if ($po->dequote($msgstr) ne "" && !$po->fuzzy()) {
		if (my($accelerator) = ($msgstr =~ /\Q$Opt_accelerator_tag\E(.)/s)) {
		    push @accelerations, { PO => $po,
					   CTXNAMES => \@ctxnames,
					   ACCELERATOR => $accelerator,
					   LINENO => $po->msgstr_begin_lineno(),
					   STRING => $msgstr,
					   EXPLAIN => "msgstr $msgstr" };
		}

		# TODO: look for accelerators in plural forms?
	    }
 	    elsif ($Opt_msgid_fallback && $po->dequote($msgid) ne "") {
 	    	if (my($accelerator) = ($msgid =~ /\Q$Opt_accelerator_tag\E(.)/s)) {
 	    	    push @accelerations, { PO => $po,
					   CTXNAMES => \@ctxnames,
 	    				   ACCELERATOR => $accelerator,
 	    				   LINENO => $po->msgid_begin_lineno(),
 	    				   STRING => $msgid,
 	    				   EXPLAIN => "msgid $msgid" };
 	    	}
 	    }

	    foreach my $acceleration (@accelerations) {
		foreach my $ctxname (@ctxnames) {
		    push(@{$accelerators{uc $acceleration->{ACCELERATOR}}{$ctxname}},
			 $acceleration);
		}
	    }
	}
    }

    foreach my $accelerator (sort keys %accelerators) {
	my $ctxhash = $accelerators{$accelerator};
	foreach my $outer_ctxname (sort keys %$ctxhash) {
	    # Cannot use "foreach my $accelerations" directly, because
	    # $accelerations would then become an alias and change to 0 below.
	    my $accelerations = $ctxhash->{$outer_ctxname};
	    if (ref($accelerations) eq "ARRAY" && @$accelerations > 1) {
		my @ctxnames_in_conflict;
		foreach my $ctxname (sort keys %$ctxhash) {
		    if (acceleration_arrays_eq($ctxhash->{$ctxname}, $accelerations)) {
			push @ctxnames_in_conflict, $ctxname;
			$ctxhash->{$ctxname} = 0;
		    }
		}
		my $ctxnames_in_conflict = join(", ", map(qq("$_"), @ctxnames_in_conflict));
		warn "$po_file_name: Accelerator conflict for \"$accelerator\" in $ctxnames_in_conflict:\n";
		foreach my $acceleration (@$accelerations) {
		    my $lineno = $acceleration->{LINENO};
		    my $explain = $acceleration->{EXPLAIN};

		    # Get a string of unique characters in the string,
		    # preferring characters that start a word.
		    # FIXME: should remove quotes and resolve \n etc.
		    my $displaystr = $acceleration->{STRING};
		    $displaystr =~ s/\Q$Opt_accelerator_tag\E//g;
		    my $suggestions = "";
		    foreach my $char ($displaystr =~ /\b(\w)/g,
				      $displaystr =~ /(\w)/g) {
			$suggestions .= $char unless $suggestions =~ /\Q$char\E/i;
		    }

		    # But don't suggest unavailable characters.
		    SUGGESTION: foreach my $char (split(//, $suggestions)) {
			foreach my $ctxname (@{$acceleration->{CTXNAMES}}) {
			    $suggestions =~ s/\Q$char\E//, next SUGGESTION
				if exists $accelerators{uc($char)}{$ctxname};
			}
		    }

		    warn "$po_file_name:$lineno: $explain\n";
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
		   if defined($Opt_accelerator_tag);
	       die "--accelerator-tag requires a single-character argument\n"
		   if length($value) != 1;
	       $Opt_accelerator_tag = $value;
	   },
	   "msgid-fallback" => \$Opt_msgid_fallback,
	   "help" => sub { pod2usage({-verbose => 1, -exitval => 0}) },
	   "version" => \&show_version)
    or exit 2;
$Opt_accelerator_tag = "~" unless defined $Opt_accelerator_tag;
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
It also tries to suggest replacements for the conflicting accelerators.

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

=item B<--msgid-fallback>

If the C<msgstr> is empty or the entry is fuzzy, check the C<msgid>
instead.  Without this option, B<check-accelerator-conflicts.pl>
completely ignores such entries.

This option also causes B<check-accelerator-conflicts.pl> not to
suggest accelerators that would conflict with a C<msgid> that was thus
checked.  Following these suggestions may lead to bad choices for
accelerators, because the conflicting C<msgid> will eventually be
shadowed by a C<msgstr> that may use a different accelerator.

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
