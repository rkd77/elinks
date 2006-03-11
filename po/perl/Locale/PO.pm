# This file has been changed from the "Standard Version" Locale-PO-0.16.
# The copyright, license, and change log are in the POD at the bottom.

use strict;
use warnings;

package Locale::PO;

use Carp;

# Internally, each Locale::PO object is a reference to a hash
# (or pseudo-hash, for Perl < 5.9) with the following keys.
# The format of the hash is subject to change; other modules
# should use the accessor methods instead.
use fields
    # Multiline strings including quotes and newlines, but excluding
    # the initial keywords and any "#~ " obsoletion marks.  Can be
    # either undef or "" if not present.  These normally do not end
    # with a newline.
    qw(msgid msgid_plural msgstr),
    # A reference to a hash where keys are numbers (as strings)
    # and values are in the same format as $self->{msgstr}.
    qw(msgstr_n),
    # Line numbers.  The file name is not currently saved.
    qw(_msgid_begin_lineno _msgstr_begin_lineno),
    # Multiline strings excluding the trailing newline and comment
    # markers, or undef if there are no such lines.
    qw(comment automatic reference),
    # Named flags.  These are kept in two formats:
    # - $self->{'_flaghash'} is undef Locale::PO has not yet parsed the flags.
    #   Otherwise, it refers to a hash where the keys are names of flags that
    #   have been set, and the values are all 1.  (The hash can be empty.)
    # - $self->{'_flagstr'} is undef if there are no flags; or a string in the
    #   same format as $self->{'automatic'}; or a reference to the same hash as
    #   $self->{'_flaghash'} if Locale::PO has changed a flag and not yet
    #   reformatted the flags as a string.
    qw(_flagstr _flaghash),
    # 1 if the entry is obsolete; undef if not.
    qw(_obsolete);

#use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
#use locale;

#require Exporter;
#require AutoLoader;

#@ISA     = qw(Exporter AutoLoader);
#@EXPORT  = qw();
our $VERSION = '0.16.kon';

# Preloaded methods go here.

sub new {
    my $this    = shift;
    my %options = @_;
    my Locale::PO $self = ref($this) ? $this : fields::new($this);
    $self->msgid( $options{'-msgid'} ) if defined( $options{'-msgid'} );
    $self->msgid_plural( $options{'-msgid_plural'} )
      if defined( $options{'-msgid_plural'} );
    $self->msgstr( $options{'-msgstr'} ) if defined( $options{'-msgstr'} );
    $self->msgstr_n( $options{'-msgstr_n'} )
      if defined( $options{'-msgstr_n'} );
    $self->comment( $options{'-comment'} ) if defined( $options{'-comment'} );
    $self->fuzzy( $options{'-fuzzy'} )     if defined( $options{'-fuzzy'} );
    $self->automatic( $options{'-automatic'} )
      if defined( $options{'-automatic'} );
    $self->reference( $options{'-reference'} )
      if defined( $options{'-reference'} );
    $self->c_format(1) if defined( $options{'-c-format'} );
    $self->c_format(1) if defined( $options{'-c_format'} );
    $self->c_format(0) if defined( $options{'-no-c-format'} );
    $self->c_format(0) if defined( $options{'-no_c_format'} );
    return $self;
}

sub msgid {
    my Locale::PO $self = shift;
    if (@_) {
        $self->{msgid} = $self->quote(shift);
        # TODO: Should this erase $self->{_msgid_begin_lineno}?
    }
    else {
        return $self->{msgid};
    }
}

sub msgid_begin_lineno {
    my Locale::PO $self = shift;
    # We should have a way to pass extra arguments (e.g. quoting
    # level) to getters, without making them behave as setters.  That
    # may require an incompatible API change, which in turn requires
    # extra methods in order to preserve compatibility.  Don't allow
    # setting msgid_begin_lineno yet; thus this method won't have to
    # be duplicated.
    croak "Setting msgid_begin_lineno is not currently allowed" if @_;
    return $self->{_msgid_begin_lineno};
}

sub msgid_plural {
    my Locale::PO $self = shift;
    @_
      ? $self->{'msgid_plural'} =
        $self->quote(shift)
      : $self->{'msgid_plural'};
}

sub msgstr {
    my Locale::PO $self = shift;
    if (@_) {
        $self->{msgstr} = $self->quote(shift);
        # TODO: Should this erase $self->{_msgstr_begin_lineno}?
    }
    else {
        return $self->{msgstr};
    }
}

sub msgstr_begin_lineno {
    my Locale::PO $self = shift;
    # We should have a way to pass extra arguments (e.g. quoting
    # level) to getters, without making them behave as setters.  That
    # may require an incompatible API change, which in turn requires
    # extra methods in order to preserve compatibility.  Don't allow
    # setting msgstr_begin_lineno yet; thus this method won't have to
    # be duplicated.
    croak "Setting msgstr_begin_lineno is not currently allowed" if @_;
    return $self->{_msgstr_begin_lineno};
}

sub msgstr_n {
    my Locale::PO $self = shift;
    if (@_) {
        my $hashref = shift;

        # check that we have a hashref.
        croak
          'Argument to msgstr_n must be a hashref: { n => "string n", ... }.'
          unless ref($hashref) eq 'HASH';

        # Check that the keys are all numbers.
        croak 'Keys to msgstr_n hashref must be numbers'
          if grep { m/\D/ } keys %$hashref;

        # Quote all the values in the hashref.
        $self->{'msgstr_n'}{$_} = $self->quote( $$hashref{$_} )
          for keys %$hashref;

    }

    return $self->{'msgstr_n'};
}

sub comment {
    my Locale::PO $self = shift;
    @_ ? $self->{'comment'} = shift: $self->{'comment'};
}

sub automatic {
    my Locale::PO $self = shift;
    @_ ? $self->{'automatic'} = shift: $self->{'automatic'};
}

sub reference {
    my Locale::PO $self = shift;
    @_ ? $self->{'reference'} = shift: $self->{'reference'};
}

# Methods whose names begin with "_" may be changed or removed in
# future versions.
sub _update_flaghash {
    my Locale::PO $self = shift;
    if (!defined($self->{'_flaghash'})) {
	my @flags;
	@flags = split(/[\s,]+/, $self->{'_flagstr'})
	    if defined $self->{'_flagstr'};
	$self->{'_flaghash'} = { map { $_ => 1 } @flags };
    }
}

# Methods whose names begin with "_" may be changed or removed in
# future versions.
sub _update_flagstr {
    my Locale::PO $self = shift;
    if (ref($self->{'_flagstr'}) eq 'HASH') {
	# GNU Gettext seems to put the "fuzzy" flag first.
	# Do the same here, in case someone's relying on it.
	# However, the other flags will be sorted differently.
	my %flags = %{$self->{'_flagstr'}};
	my @flags = ();
	push @flags, 'fuzzy' if delete $flags{'fuzzy'};
	push @flags, sort { $a cmp $b } keys %flags;
	$self->{'_flagstr'} = (@flags ? join(', ', @flags) : undef);
    }
}

# Methods whose names begin with "_" may be changed or removed in
# future versions.
sub _flag {
    my Locale::PO $self = shift;
    my $name = shift;
    $self->_update_flaghash();
    if (@_) { # set or clear the flag
	$self->{'_flagstr'} = $self->{'_flaghash'};
	if (shift) {
	    $self->{'_flaghash'}{$name} = 1;
	    return 1;
	}
	else {
	    delete $self->{'_flaghash'}{$name};
	    return "";
	}
    }
    else { # check the flag
	return exists $self->{'_flaghash'}{$name};
    }
}

# Methods whose names begin with "_" may be changed or removed in
# future versions.
sub _tristate {
    my Locale::PO $self = shift;
    my $name = shift;
    $self->_update_flaghash();
    if (@_) { # set or clear the flags
	$self->{'_flagstr'} = $self->{'_flaghash'};
	my $val = shift;
	if (!defined($val) || $val eq "") {
	    delete $self->{'_flaghash'}{"$name"};
	    delete $self->{'_flaghash'}{"no-$name"};
	    return undef;
	}
	elsif ($val) {
	    $self->{'_flaghash'}{"$name"} = 1;
	    delete $self->{'_flaghash'}{"no-$name"};
	    return 1;
	}
	else {
	    delete $self->{'_flaghash'}{"$name"};
	    $self->{'_flaghash'}{"no-$name"} = 1;
	    return 0;
	}
    }
    else { # check the flags
	return 1 if $self->{'_flaghash'}{"$name"};
	return 0 if $self->{'_flaghash'}{"no-$name"};
	return undef;
    }
}

sub fuzzy {
    my Locale::PO $self = shift;
    return $self->_flag('fuzzy', @_);
}

sub c_format {
    my Locale::PO $self = shift;
    return $self->_tristate('c-format', @_);
}

sub php_format {
    my Locale::PO $self = shift;
    return $self->_tristate('php-format', @_);
}

sub obsolete {
    my Locale::PO $self = shift;
    @_ ? $self->{_obsolete} = shift : $self->{_obsolete};
}

# Methods whose names begin with "_" may be changed or removed in
# future versions.
sub _normalize_str {
    my $self     = shift;       # can be called as a class method
    my $string   = shift;
    my $dequoted = $self->dequote($string);

    # This isn't quite perfect, but it's fast and easy
    if ( $dequoted =~ /(^|[^\\])(\\\\)*\\n./ ) {

        # Multiline
        my $output;
        my @lines;
        $output = '""' . "\n";
        @lines = split( /\\n/, $dequoted, -1 );
        my $lastline = pop @lines;    # special treatment for this one
        foreach (@lines) {
            $output .= $self->quote("$_\\n") . "\n";
        }
        $output .= $self->quote($lastline) . "\n" if $lastline ne "";
        return $output;
    }
    else {

        # Single line
        return "$string\n";
    }
}

sub dump {
    my Locale::PO $self = shift;
    my $dump;
    $dump .= $self->_dump_multi_comment( $self->comment, "# " )
      if defined( $self->comment );
    $dump .= $self->_dump_multi_comment( $self->automatic, "#. " )
      if defined( $self->automatic );
    $dump .= $self->_dump_multi_comment( $self->reference, "#: " )
      if defined( $self->reference );
    $self->_update_flagstr();
    $dump .= $self->_dump_multi_comment( $self->{'_flagstr'}, "#, " )
      if defined( $self->{'_flagstr'} );
    $dump .= "msgid " . $self->_normalize_str( $self->msgid )
      if $self->msgid;
    $dump .= "msgid_plural " . $self->_normalize_str( $self->msgid_plural )
      if $self->msgid_plural;

    $dump .= "msgstr " . $self->_normalize_str( $self->msgstr )
	if $self->msgstr;

    if ( my $msgstr_n = $self->msgstr_n ) {
        $dump .= "msgstr[$_] " . $self->_normalize_str( $$msgstr_n{$_} )
          for sort { $a <=> $b } keys %$msgstr_n;
    }

    $dump =~ s/^(?!#)/#~ /gm if $self->{_obsolete};
    $dump .= "\n";
    return $dump;
}

# Methods whose names begin with "_" may be changed or removed in
# future versions.
sub _dump_multi_comment {
    my $self    = shift;        # can be called as a class method
    my $comment = shift;
    my $leader  = shift;
    my $chopped = $leader;
    chop($chopped);
    my $result = $leader . $comment;
    $result =~ s/\n/\n$leader/g;
    $result =~ s/^$leader$/$chopped/gm;
    $result .= "\n";
    return $result;
}

# Quote a string properly
sub quote {
    my $self   = shift;         # can be called as a class method
    my $string = shift;
    $string =~ s/"/\\"/g;
    return "\"$string\"";
}

sub dequote {
    my $self   = shift;         # can be called as a class method
    my $string = shift;
    $string =~ s/^"(.*)"/$1/gm;
    $string =~ s/\\"/"/g;
    return $string;
}

sub save_file_fromarray {
    my $self = shift;           # normally called as a class method
    $self->save_file( @_, 0 );
}

sub save_file_fromhash {
    my $self = shift;           # normally called as a class method
    $self->save_file( @_, 1 );
}

sub save_file {
    my $self    = shift;        # normally called as a class method
    my $file    = shift;
    my $entries = shift;
    my $ashash  = shift;
    my $err     = undef;
    open( OUT, ">$file" ) or return undef;
    if ($ashash) {
        foreach ( sort keys %$entries ) {
            print OUT $entries->{$_}->dump or $err=$!, last;
        }
    }
    else {
        foreach (@$entries) {
            print OUT $_->dump or $err=$!, last;
        }
    }
    close OUT or $err=$!;
    if ($err) { $!=$err; return undef }
    else { return 1 }
}

sub load_file_asarray {
    my $self = shift;           # normally called as a class method
    my $file = shift;
    my @entries;
    my $po;
    my $last_buffer;
    use constant {
        # Nothing other than comments has been seen for $po yet.
        # $po may also be undef.
        # On comment: save it, and stay in this state.
        # On msgid: save it, and switch to STATE_STRING.
        # On some other string: warn that the msgid is missing,
        #   and redo in STATE_STRING.
        STATE_COMMENT => 1,

        # A non-comment string has been seen for $po.
        # $po can not be undef.
        # If $po->{msgid} is undef, that has already been warned about.
        # On comment: close the entry, and redo in STATE_COMMENT.
        # On msgid: close the entry, and redo in STATE_COMMENT.
        # On some other string: save it, and stay in this state.
        STATE_STRING => 2,
    };
    my $state = STATE_COMMENT;
    local $/ = "\n";
    local $_;

    # "my sub" not yet implemented
    my $check_and_push_entry = sub {
        # Call this only if defined($po).
        warn "$file:$.: Expected msgid\n"
            unless defined $po->{msgid} or $state == STATE_STRING;
        if (defined $po->{msgid_plural}) {
            warn "$file:$.: Expected msgstr[n]\n"
                unless defined $po->{msgstr_n};
        }
        else {
            warn "$file:$.: Expected msgstr\n"
                unless defined $po->{msgstr};
        }

        push( @entries, $po);
        $po = undef;
        $last_buffer = undef;
    };
        
    open( IN, "<$file" ) or return undef;
    LINE: while (<IN>) {
        chomp;
        my $obsolete = s/^#~ ?//;
        next LINE if /^$/;

        if (/^"/) {
            # Continued string.  This is very common, so check it first.
            warn("$file:$.: There is no string to be continued\n"), next LINE
                unless defined $last_buffer;
            $$last_buffer .= "\n$_";
        }
        elsif (/^#([,.:]?)()$/ or /^#([,.:]?) (.*)$/) {
            &$check_and_push_entry if $state == STATE_STRING;
            $state = STATE_COMMENT;
            $po = new Locale::PO unless defined $po;

            my $comment;
            if    ($1 eq "")  { $comment = \$po->{comment} }
            elsif ($1 eq ",") { $comment = \$po->{_flagstr} }
            elsif ($1 eq ".") { $comment = \$po->{automatic} }
            elsif ($1 eq ":") { $comment = \$po->{reference} }
            else { warn "Bug: did not recognize '$1'"; next LINE }

            if (defined( $$comment )) { $$comment .= "\n$2" }
            else { $$comment = $2 }
        }
        elsif (/^msgid (.*)$/) {
            &$check_and_push_entry if $state == STATE_STRING;
            $state = STATE_STRING;
            $po = new Locale::PO unless defined $po;
            $last_buffer = \($po->{msgid} = $1);
            $po->{_msgid_begin_lineno} = $.;
        }
        elsif (/^msgid_plural (.*)$/) {
            if ($state == STATE_COMMENT) {
                warn "$file:$.: Expected msgid\n";
                $po = new Locale::PO unless defined $po;
                $state = STATE_STRING;
            }
            if (defined $po->{msgid_plural}) {
                warn "$file:$.: Replacing previous msgid_plural\n";
            }
            elsif (defined $po->{msgstr}) {
                warn "$file:$.: Should not have msgid_plural with msgstr\n";
            }
            $last_buffer = \($po->{msgid_plural} = $1);
        }
        elsif (/^msgstr (.*)$/) {
            if ($state == STATE_COMMENT) {
                warn "$file:$.: Expected msgid\n";
                $po = new Locale::PO unless defined $po;
                $state = STATE_STRING;
            }
            if (defined $po->{msgstr}) {
                warn "$file:$.: Replacing previous msgstr\n";
            }
            elsif (defined $po->{msgid_plural}) {
                warn "$file:$.: Should not have msgstr with msgid_plural\n";
            }
            elsif (defined $po->{msgstr_n}) {
                warn "$file:$.: Should not have msgstr with msgstr[n]\n";
            }
            $last_buffer = \($po->{msgstr} = $1);
            $po->{_msgstr_begin_lineno} = $.;
        }
        elsif (/^msgstr\[(\d+)\] (.*)$/) {
            if ($state == STATE_COMMENT) {
                warn "$file:$.: Expected msgid\n";
                $po = new Locale::PO unless defined $po;
                $state = STATE_STRING;
            }
            if (defined $po->{msgstr_n}) {
                warn "$file:$.: Replacing previous msgstr[$1]\n"
                    if defined $po->{msgstr_n}{$1};
            }
            elsif (defined $po->{msgstr}) {
                warn "$file:$.: Should not have msgstr[n] with msgstr\n";
            }
            $last_buffer = \($po->{msgstr_n}{$1} = $2);
        }
        else {
            warn "$file:$.: Ignoring strange line: $_\n";
        }
        $po->{_obsolete} = 1 if $obsolete && defined $po;
    }
    &$check_and_push_entry if defined $po;
    close IN;
    return \@entries;
}

sub load_file_ashash {
    my $self = shift;           # normally called as a class method
    my $file = shift;
    my $entries = $self->load_file_asarray( $file );
    my %entries;

    ENTRY: foreach my $po (@$entries) {
        my $msgid = $po->msgid;
        if (!defined( $msgid )) {
            # The entry had no msgid line in the input file.
            # &load_file_asarray already warned about that.  Such
            # entries have no identity and cannot be put in the hash.
            next ENTRY;
        }
        # The hash keys were quoted strings in Locale-PO-0.16,
        # so keep them that way.  However, if the same string has
        # been quoted in two different ways, there should not be
        # two different hash entries.  Canonicalize the key.
        my $key = $po->quote( $po->dequote( $msgid ));
        if (exists( $entries{$key} ) && $entries{$key}->msgstr =~ /\w/) {
            # This msgid has a translation already.  Don't replace it.
            next ENTRY;
        }
        $entries{$key} = $po;
    }

    return \%entries;
}

sub load_file {
    my $self   = shift;         # normally called as a class method
    my $file   = shift;
    my $ashash = shift;

    if ($ashash) {
        return $self->load_file_ashash($file);
    }
    else {
        return $self->load_file_asarray($file);
    }
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

# Below is the stub of documentation for your module. You better edit it!

=head1 NAME

Locale::PO - Perl module for manipulating .po entries from GNU gettext

=head1 SYNOPSIS

  use Locale::PO;

  $po = new Locale::PO([-option=>value,...])

  # get fields                         # set fields
  $fully_quoted = $po->msgid;          $po->msgid($backslashed);
  $fully_quoted = $po->msgid_plural;   $po->msgid_plural($backslashed);
  $fully_quoted = $po->msgstr;         $po->msgstr($backslashed);
  $lineno = $po->msgstr_begin_lineno;
  $fully_quoted_href = $po->msgstr_n;  $po->msgstr_n($backslashed_href);
  $string = $po->comment;              $po->comment($string);
  $string = $po->automatic;            $po->automatic($string);
  $string = $po->reference;            $po->reference($string);
  $flag = $po->fuzzy;                  $po->fuzzy($flag);
  $flag = $po->obsolete;               $po->obsolete($flag);
  $tristate = $po->c_format;           $po->c_format($tristate);
  $tristate = $po->php_format;         $po->php_format($tristate);

  print $po->dump;

  $fully_quoted_string = $po->quote($backslashed_string);
  $backslashed_string = $po->dequote($fully_quoted_string);

  $aref = Locale::PO->load_file_asarray($filename);
  $href = Locale::PO->load_file_ashash($filename);
  $ref = Locale::PO->load_file($filename, $ashash);
  Locale::PO->save_file_fromarray($filename, $aref);
  Locale::PO->save_file_fromhash($filename, $href);
  Locale::PO->save_file($filename, $ref, $fromhash);

=head1 DESCRIPTION

This module simplifies management of GNU gettext .po files and is an
alternative to using emacs po-mode. It provides an object-oriented
interface in which each entry in a .po file is a Locale::PO object.

=head2 Levels of Quoting

When you use methods of Locale::PO, you need to distinguish between
three possible levels of quoting strings.  These levels are described
below.  The descriptions also list how the following sample msgid
string would be quoted in each level.

  msgid "The characters /\\ denote the \"AND\" operator.\n"
  "Please enter all numbers in octal."

=over

=item FULLY-QUOTED

The same format as in a PO file: the string may consist of multiple
lines, and each line has double-quote characters around it and may
contain backslash sequences.  Any double-quote or backslash characters
that should be part of the data must be escaped with backslashes.

The example in Perl syntax: C<qq("The characters /\\\\ denote the
\\"AND\\" operator.\\n"\n"Please enter all numbers in octal.")>

=item BACKSLASHED

The string may consist of multiple lines.  The lines do not have
double-quote characters around them, but they may contain backslash
sequences.  Any backslash characters that should be part of the data
must be escaped with backslashes, but double-quote characters must not.

The example in Perl syntax: C<qq(The characters /\\\\ denote the
"AND" operator.\\n\nPlease enter all numbers in octal.)>

=item NOT-QUOTED

This is the format that a C program would pass to the C<gettext>
function or output to a terminal device.  Any remaining quotes are
part of the data itself, and backslash escapes have been replaced
with control characters.  Locale::PO does not currently support
this quoting level.

The example in Perl syntax: C<qq(The characters /\\ denote the "AND"
operator.\nPlease enter all numbers in octal.)>

=back

=head1 METHODS

=over 4

=item new

  my Locale::PO $po = new Locale::PO;
  my Locale::PO $po = new Locale::PO(%options);

Create a new Locale::PO object to represent a po entry.
You can optionally set the attributes of the entry by passing 
a list/hash of the form:

 -option=>value, -option=>value, etc.

Where options are msgid, msgstr, comment, automatic, reference, fuzzy,
c-format, and no-c-format. See accessor methods below.  Currently, you
cannot set the "obsolete" or "php-format" flags via options.

To generate a po file header, add an entry with an empty
msgid, like this:

   $po = new Locale::PO(-msgid=>'', -msgstr=>
	"Project-Id-Version: PACKAGE VERSION\\n" .
	"PO-Revision-Date: YEAR-MO-DA HO:MI +ZONE\\n" .
	"Last-Translator: FULL NAME <EMAIL@ADDRESS>\\n" .
	"Language-Team: LANGUAGE <LL@li.org>\\n" .
	"MIME-Version: 1.0\\n" .
	"Content-Type: text/plain; charset=CHARSET\\n" .
	"Content-Transfer-Encoding: ENCODING\\n");

Note that the C<msgid> and C<msgstr> must be in BACKSLASHED form.

=item msgid

  $fully_quoted_string = $po->msgid;
  # $backslashed_string = $po->dequote($fully_quoted_string);
  $po->msgid($backslashed_string);

Set or get the untranslated string from the object.

The value is C<undef> if there is no such string for this message, not
even an empty string.  If the message was loaded from a PO file, this
can occur if there are comments after the last C<msgid> line.  The
loading functions warn about such omissions.

This method expects the new string in BACKSLASHED form
but returns the current string in FULLY-QUOTED form.

=item msgid_begin_lineno

  $line_number = $po->msgid_begin_lineno;

Get the line number at which the C<msgid> string begins in the PO file.
This is undef if the entry was not loaded from a file.
There is currently no setter method for this field.

=item msgid_plural

  $fully_quoted_string = $po->msgid_plural;
  # $backslashed_string = $po->dequote($fully_quoted_string);
  $po->msgid_plural($backslashed_string);

Set or get the untranslated plural string from the object.  The value
is C<undef> if there is no plural form for this message.

This method expects the new string in BACKSLASHED form
but returns the current string in FULLY-QUOTED form.

=item msgstr

  $fully_quoted_string = $po->msgstr;
  # $backslashed_string = $po->dequote($fully_quoted_string);
  $po->msgstr($backslashed_string);

Set or get the translated string from the object.

If the string has plural forms, then they are instead accessible via
C<msgstr_n>, and C<msgstr> normally returns C<undef>.  However, if the
entry has been loaded from an incorrectly formatted PO file, then it
is also possible that both C<msgstr> and C<msgstr_n> return C<undef>,
or that they both return defined values.  The loading functions warn
about such transgressions.

This method expects the new string in BACKSLASHED form
but returns the current string in FULLY-QUOTED form.

=item msgstr_begin_lineno

  $line_number = $po->msgstr_begin_lineno;

Get the line number at which the C<msgstr> string begins in the PO file.
This is undef if the entry was not loaded from a file.
There is currently no setter method for this field.

=item msgstr_n

  $fully_quoted_hashref = $po->msgstr_n;
  # $backslashed_hashref = { %$fully_quoted_hashref };
  # $_ = $po->dequote($_) foreach values(%$backslashed_hashref);
  $po->msgstr_n($backslashed_hashref);

Get or set the translations if there are plurals involved. Takes and
returns a hashref where the keys are the 'N' case and the values are
the strings. eg:

    $po->msgstr_n(
        {
            0 => 'found %d plural translations',
            1 => 'found %d singular translation',
        }
    );

The value C<undef> should be treated the same as an empty hash.
Callers should neither modify the hash to which the returned reference
points, nor assume it to remain valid if C<msgstr_n> is later called
to install a new set of translations.

This method expects the new strings in BACKSLASHED form
but returns the current strings in FULLY-QUOTED form.

=item comment

  $string = $po->comment;
  $po->comment($string);

Set or get translator comments from the object.

If there are no such comments, then the value is C<undef>.  Otherwise,
the value is a string that contains the comment lines delimited with
"\n".  The string includes neither the S<"# "> at the beginning of
each comment line nor the newline at the end of the last comment line.

=item automatic

  $string = $po->automatic;
  $po->automatic($string);

Set or get automatic comments from the object (inserted by 
emacs po-mode or xgettext).

If there are no such comments, then the value is C<undef>.  Otherwise,
the value is a string that contains the comment lines delimited with
"\n".  The string includes neither the S<"#. "> at the beginning of
each comment line nor the newline at the end of the last comment line.

=item reference

  $string = $po->reference;
  $po->reference($string);

Set or get reference marking comments from the object (inserted
by emacs po-mode or gettext).

If there are no such comments, then the value is C<undef>.  Otherwise,
the value is a string that contains the comment lines delimited with
"\n".  The string includes neither the S<"#: "> at the beginning of
each comment line nor the newline at the end of the last comment line.

=item fuzzy

  $flag = $po->fuzzy;
  $po->fuzzy($flag);

Set or get the fuzzy flag on the object ("check this translation").
When setting, use 1 to turn on fuzzy, and 0 to turn it off.

=item obsolete

  $flag = $po->obsolete;
  $po->obsolete($flag);

Set or get the obsolete flag on the object ("no longer used").
When setting, use 1 to turn on obsolete, and 0 to turn it off.

=item c_format

  $tristate = $po->obsolete;
  $po->obsolete($tristate);

Set or get the c-format or no-c-format flag on the object.
This can take 3 values: 1 implies c-format, 0 implies no-c-format,
and blank or undefined implies neither.

=item php_format

  $tristate = $po->php_format;
  $po->php_format($tristate);

Set or get the php-format or no-php-format flag on the object.
This can take 3 values: 1 implies php-format, 0 implies no-php-format,
and blank or undefined implies neither.

=item dump

  print $po->dump;

Returns the entry as a string, suitable for output to a po file.

=item quote

  $fully_quoted_string = $po->quote($backslashed_string);

Converts a string from BACKSLASHED to FULLY-QUOTED form.
Specifically, the quoted string will have all existing double-quote
characters escaped by backslashes, and each line will be enclosed in
double quotes.  Preexisting backslashes will not be doubled.

=item dequote

  $backslashed_string = $po->dequote($fully_quoted_string);

Converts a string from FULLY-QUOTED to BACKSLASHED form.
Specifically, it first removes the double-quote characters that
surround each line.  After this, each remaining double-quote character
should have a backslash in front of it; the method then removes those
backslashes.  Backslashes in any other position will be left intact.

=item load_file_asarray

  $arrayref = Locale::PO->load_file_asarray($filename);

Given the filename of a po-file, reads the file and returns a
reference to a list of Locale::PO objects corresponding to the contents of
the file, in the same order.

If the file cannot be read, then this method returns C<undef>, and you
can check C<$!> for the actual error.  If the file does not follow the
expected syntax, then the method generates warnings but keeps going.
Other errors (e.g. out of memory) may result in C<die> being called,
of course.

=item load_file_ashash

  $hashref = Locale::PO->load_file_ashash($filename);

Given the filename of a po-file, reads the file and returns a
reference to a hash of Locale::PO objects corresponding to the contents of
the file. The hash keys are the untranslated strings (in BACKSLASHED form),
so this is a cheap way to remove duplicates. The method will prefer to keep
entries that have been translated.

This handles errors in the same way as C<load_file_asarray> does.

=item load_file

  $ref = Locale::PO->load_file($filename, $ashash);

This method behaves as C<load_file_asarray> if the C<$ashash>
parameter is 0, or as C<load_file_ashash> if C<$ashash> is 1.
Your code will probably be easier to understand if you call either
of those methods instead of this one.

=item save_file_fromarray

  $ok = Locale::PO->save_file_fromarray($filename, \@objects)

Given a filename and a reference to a list of Locale::PO objects,
saves those objects to the file, creating a po-file.

Returns true if successful.
Returns C<undef> and sets C<$!> if an I/O error occurs.
Dies if a serious error occurs.

=item save_file_fromhash

  $ok = Locale::PO->save_file_fromhash($filename, \%objects);

Given a filename and a reference to a hash of Locale::PO objects,
saves those objects to the file, creating a po-file. The entries
are sorted alphabetically by untranslated string.

Returns true if successful.
Returns C<undef> and sets C<$!> if an I/O error occurs.
Dies if a serious error occurs.

=item save_file

  $ok = Locale::PO->save_file($filename, $ref, $fromhash);

This method behaves as C<save_file_fromarray> if the C<$fromhash>
parameter is 0, or as C<save_file_fromhash> if C<$fromhash> is 1.
Your code will probably be easier to understand if you call either
of those methods instead of this one.

=back

=head1 AUTHOR

Alan Schwartz, alansz@pennmush.org

=head1 DIFFERENCES FROM Locale-PO-0.16

List of changes in this file, as stipulated in section 3. of the
L<"Artistic License"|perlartistic> and subsection 2. a) of L<GNU
General Public License version 2|perlgpl>:

=over 4

=item Z<>2006-01-06  Kalle Olavi Niemitalo  <kon@iki.fi>

Appended ".kon" to C<$VERSION>.
Use C<fields>, and C<my Locale::PO> where applicable.

POD changes:
Added the copyright notice (from F<README>) and this history.
Corrected a typo in the documentation.
Documented quoting in the C<msgid>, C<msgid_plural>, C<msgstr>, and C<msgstr_n> methods.
Documented newlines in the C<comment>, C<automatic>, and C<reference> methods.

=item Z<>2006-01-07  Kalle Olavi Niemitalo  <kon@iki.fi>

POD changes:
Documented the C<php_format> method.

=item Z<>2006-01-08  Kalle Olavi Niemitalo  <kon@iki.fi>

POD changes:
Greatly expanded the L</BUGS> section.
Reformatted this list of changes.

=item Z<>2006-02-05  Kalle Olavi Niemitalo  <kon@iki.fi>

Added comments about the fields of Locale::PO objects.
The C<load_file> function binds C<$/> and C<$_> dynamically.
Renamed C<normalize_str> to C<_normalize_str>, and C<dump_multi_comment> to C<_dump_multi_comment>.

POD changes:
Documented C<load_file> and C<save_file>.

=item Z<>2006-02-11  Kalle Olavi Niemitalo  <kon@iki.fi>

Rewrote the file parser.  It now warns about various inconsistencies, and no longer relies on empty lines.
The parser writes directly to fields of Locale::PO objects, preserving internal newlines in strings.  Changed C<dequote> to process all lines of multi-line strings.
The parser supports obsolete entries.  Added the C<_obsolete> field and the C<obsolete> method.  C<dump> comments the entry out if it is obsolete.
The parser preserves the original string of flags and scans it more carefully.  Added the C<_flag> field.  Implemented "no-php-format".
C<dump> dumps comments even if they are C<eq "0">.

POD changes:
Documented the bugs fixed with the changes above.
Documented levels of quoting, and the exact behaviour of C<quote> and C<dequote>.
Documented the C<obsolete> method.
Documented error handling in C<load_file_asarray> and C<load_file_ashash>.
Documented when C<msgid>, C<msgstr> etc. can return C<undef>.

=item Z<>2006-02-12  Kalle Olavi Niemitalo  <kon@iki.fi>

C<save_file> returns C<undef> and remembers C<$!> if C<print> fails.
C<load_file_asarray> saves line numbers of C<msgid> and C<msgstr>.  New fields C<_msgid_begin_lineno> and C<_msgstr_begin_lineno>; new methods C<msgid_begin_lineno> and C<msgstr_begin_lineno>.

POD changes:
Revised the synopses of the methods, paying attention to levels of quoting.
Repeat the synopsis above the description of each method.
Never write C<< CZ<><Locale::PO> >>; it looks bad in B<pod2text>.
Documented that C<msgstr> normally returns C<undef> if there are plurals.
Documented the new methods C<msgid_begin_lineno> and C<msgstr_begin_lineno>.

=item Z<>2006-02-18  Kalle Olavi Niemitalo  <kon@iki.fi>

Locale::PO now preserves unrecognized flags, although there is still no documented way to access them.  It also preserves the order of flags, if no flags are modified.  Replaced the C<fuzzy>, C<c_format>, and C<php_format> fields with C<_flaghash>, and renamed the C<_flag> field to C<_flagstr>.
Flag-setting functions silently map unsupported values (e.g. 42) to supported ones (e.g. 1), which they also return.
The C<c_format> and C<php_format> methods treat empty strings as C<undef>, rather than as 0.
Names of flags are case-sensitive, like in GNU Gettext.

POD changes:
Unlisted the bugs that have now been fixed.

=item Z<>2006-02-19  Kalle Olavi Niemitalo  <kon@iki.fi>

The C<dump> method doesn't output an C<msgid> if there isn't one. 

=back

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2000-2004 Alan Schwartz <alansz@pennmush.org>.
All rights reserved.  This program is free software; you can
redistribute it and/or modify it under the same terms as Perl itself.

=head1 BUGS

If you C<load_file> then C<save_file>, the output file may have slight
cosmetic differences from the input file (an extra blank line here or there).

The C<quote> and C<dequote> methods assume Perl knows the encoding
of the string.  If it doesn't, they'll treat each 0x5C byte as a
backslash even if it's actually part of a multibyte character.
Therefore, Locale::PO should parse the charset parameter from the
header entry, and decode the strings with that.  It is unclear whether
the charset must be decoded even before the newlines and quotes are
parsed; this would mainly be a requirement with UTF-16, which GNU
Gettext doesn't support.

=head2 Almost Bugs

C<msgid_begin_lineno> and C<msgstr_begin_lineno> are read-only.
Perhaps there should also be ways to get the line numbers of the
other strings.  Probably not line numbers of comments, though.

The C<msgid>, C<msgid_plural>, C<msgstr>, and C<msgstr_n> methods
output FULLY-QUOTED strings, but they expect BACKSLASHED strings as
input.  It would be better to have both FULLY-QUOTED or both
BACKSLASHED; or perhaps C<< $po->msgid(-level => 'BACKSLASHED') >>.

Locale::PO discards all types of comments it does not recognize.
The B<msgmerge> program of GNU gettext-tools 0.14.3 does the same,
so perhaps it's not a problem.

=head1 SEE ALSO

xgettext(1).

=cut
