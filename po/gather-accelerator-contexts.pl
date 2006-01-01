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
