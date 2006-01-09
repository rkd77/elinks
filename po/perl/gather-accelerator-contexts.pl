#! /usr/bin/perl
use strict;
use warnings;
use Locale::PO qw();

{
    package Contextline;
    use fields qw(lineno contexts);
    sub new {
        my($self, $lineno, $contexts) = @_;
        $self = fields::new($self) unless ref $self;
	$self->{lineno} = $lineno;
	$self->{contexts} = $contexts;
	return $self;
    }
}

# Each key is a file name.
# Each value is a reference to an array of references to Contextline
# pseudo-hashes.  The array is in ascending order by {lineno}.
my %Srcfiles;

# Scan the $srcfile for gettext_accelerator_context directives,
# cache the result in %Srcfiles, and return it in that format.
sub contextlines ($$)
{
    my($top_srcdir, $srcfile) = @_;
    return $Srcfiles{$srcfile} if exists($Srcfiles{$srcfile});

    local $_;
    my @contextlines = ();
    my @prevctxs;
    open my $srcfd, "<", "$top_srcdir/$srcfile" or die "$top_srcdir/$srcfile: $!";
    while (<$srcfd>) {
	chomp;
	if (/^\}/ && @prevctxs) {
	    push @contextlines, Contextline->new($., [@prevctxs = ()]);
	}
	if (my($contexts) = /\[gettext_accelerator_context\(([^()]*)\)\]/) {
	    my @contexts = grep { $_ ne "" } split(/\s*,\s*/, $contexts);
	    foreach (@contexts) { s/^\./${srcfile}:/ }
	    warn "$srcfile:$.: Previous context not closed\n"
		if @prevctxs && @contexts;
	    warn "$srcfile:$.: Context already closed\n"
		if !@prevctxs && !@contexts;
	    push @contextlines, Contextline->new($., [@prevctxs = @contexts]);
	} elsif (/gettext_accelerator_context/) {
	    warn "$srcfile:$.: Suspicious non-directive: $_\n";
	}
    }
    warn "$srcfile:$.: Last context not closed\n" if @prevctxs;

    return $Srcfiles{$srcfile} = \@contextlines;
}

sub contexts ($$$)
{
    my($top_srcdir, $srcfile, $lineno) = @_;
    # Could use a binary search here.
    my $contextlines = contextlines($top_srcdir, $srcfile);
    my @contexts = ();
    foreach my Contextline $contextline (@{$contextlines}) {
	return @contexts if $contextline->{lineno} > $lineno;
	@contexts = @{$contextline->{contexts}};
    }
    return ();
}

sub format_contexts (@)
{
    if (@_) {
	return "#. accelerator_context(" . join(", ", @_) . ")\n";
    } else {
	return "";
    }
}

my($top_srcdir, $pofile) = @ARGV;
my $pos = Locale::PO->load_file_asarray($pofile) or die "$pofile: $!";
foreach my $po (@$pos) {
    my $automatic = $po->automatic();
    $automatic =~ s/^\[gettext_accelerator_context\(.*(?:\n|\z)//mg
	if defined($automatic);

    if ($po->msgid() =~ /\~/) {
	my @po_contexts = ();
	foreach my $ref (split(' ', $po->reference())) {
	    my @parts = split(/\:/, $ref);
	    warn "weird reference: $ref\n", next unless @parts == 2;
	    my @ref_contexts = contexts($top_srcdir, $parts[0], $parts[1]);
	    if (@ref_contexts) {
		push @po_contexts, grep { $_ ne "IGNORE" } @ref_contexts;
	    } else {
		warn "$ref: No accelerator context for msgid " . $po->msgid() . "\n";
	    }
	}
	if (@po_contexts) {
	    # sort and uniquify
	    @po_contexts = sort keys %{{map { $_ => 1 } @po_contexts}};
	    $automatic .= "\n" if defined($automatic) and $automatic ne "";
	    $automatic .= "accelerator_context(" . join(", ", @po_contexts) . ")";
	}
    }
    $po->automatic($automatic);
}
Locale::PO->save_file_fromarray($pofile, $pos) or die "$pofile: $!";

__END__

=head1 NAME

gather-accelerator-contexts.pl - Augment a PO file with information
for detecting accelerator conflicts.

=head1 SYNOPSIS

B<gather-accelerator-contexts.pl> I<top_srcdir> F<I<program>.pot>

=head1 DESCRIPTION

B<gather-accelerator-contexts.pl> is part of a framework that detects
conflicting accelerator keys in Gettext PO files.  A conflict is when
two items in the same menu or two buttons in the same dialog box use
the same accelerator key.

The PO file format does not normally include any information on which
strings will be used in the same menu or dialog box.
B<gather-accelerator-contexts.pl> adds this information in the form of
"accelerator_context" comments, which B<check-accelerator-contexts.pl>
then parses in order to detect the conflicts.

B<gather-accelerator-contexts.pl> first reads the F<I<program>.pot>
file named on the command line.  This file must include "#:" comments
that point to the source files from which B<xgettext> extracted each
msgid.  B<gather-accelerator-contexts.pl> then scans those source
files for context information and rewrites F<I<program>.pot> to
include the "accelerator_context" comments.  Finally, the standard
tool B<msgmerge> can be used to copy the added comments to all the
F<I<language>.po> files.

It is best to run B<gather-accelerator-contexts.pl> immediately after
B<xgettext> so that the source references will be up to date.

=head2 Contexts

Whenever a source file refers to an C<msgid> that includes an
accelerator key, it must assign one or more named B<contexts> to it.
The C<msgstr>s in the PO files inherit these contexts.  If multiple
C<msgstr>s use the same accelerator (case insensitive) in the same
context, that's a conflict and can be detected automatically.

If the same C<msgid> is used in multiple places in the source code,
and those places assign different contexts to it, then all of those
contexts will apply.

The names of contexts consist of C identifiers delimited with periods.
The identifier is typically the name of a function that sets up a
dialog, or the name of an array where the items of a menu are listed.
There is a special feature for file-local identifiers (C<static> in C):
if the name begins with a period, then the period will be replaced
with the name of the source file and a colon.  The name "IGNORE" is
reserved.

If a menu is programmatically generated from multiple parts, of which
some are never used together, so that it is safe to use the same
accelerators in them, then it is necessary to define multiple contexts
for the same menu.

=head2 How to define contexts in source files

The contexts are defined with "gettext_accelerator_context" comments
in source files.  These comments delimit regions where all C<msgid>s
containing tildes are given the same contexts.  There must be one
special comment at the top of the region; it lists the contexts
assigned to that region.  The region automatically ends at the end of
the function (found with regexp C</^\}/>), but it can also be closed
explicitly with another special comment.  The comments are formatted
like this:

  /* [gettext_accelerator_context(foo, bar, baz)]
       begins a region that uses the contexts "foo", "bar", and "baz".
       The comma is the delimiter; whitespace is optional.

     [gettext_accelerator_context()]
       ends the region.  */

B<gather-accelerator-contexts.pl> removes from F<I<program>.pot> any
"gettext_accelerator_context" comments that B<xgettext --add-comments>
may have copied there.  

If B<gather-accelerator-contexts.pl> does not find any contexts for
some use of an C<msgid> that seems to contain an accelerator (because
it contains a tilde), it warns.  If the tilde does not actually mark
an accelerator (e.g. in "~/.bashrc"), the warning can be silenced by
specifying the special context "IGNORE", which
B<gather-accelerator-contexts.pl> otherwise ignores.

=head1 ARGUMENTS

=over

=item I<top_srcdir>

The directory to which the source references in "#:" lines are
relative.

=item F<I<program>.pot>

The file to augment with context information.
B<gather-accelerator-contexts.pl> first reads this file and then
overwrites it.

Although this documentation keeps referring to F<I<program>.pot>,
you can also use B<gather-accelerator-contexts.pl> on an already
translated F<I<language>.po>.  However, that will only work correctly
if the source references in the "#:" lines are still up to date.

=back

=head1 BUGS

B<gather-accelerator-contexts.pl> assumes that accelerator keys in
translatable strings are marked with the tilde (~) character.  This
should be configurable, as in B<msgfmt --check-accelerators="~">.

B<gather-accelerator-contexts.pl> assumes that source files are in
the C programming language: specifically, that a closing brace at
the beginning of a line marks the end of a function.

B<gather-accelerator-contexts.pl> doesn't check whether the
"gettext_accelerator_context" comments actually are comments.

There should be a way to specify a source path, rather than just a
single I<top_srcdir> directory.

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

L<check-accelerator-contexts.pl>, C<xgettext(1)>, C<msgmerge(1)>
