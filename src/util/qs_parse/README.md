# qs_parse #

## Description ##

A set of simple and easy functions for parsing URL query strings, such as
those generated in an HTTP GET form submission.


## How to Use ##

These qs_* functions can be used in two different ways:

`qs_parse()`/`qs_k2v()`: A faster version that requires a pre-processing stage
and **is destructive to the query string**

  * Better for repeated lookups of k/v pairs
  * Does all decoding/processing in-place, so no memory copying
  * Requires an array of pointers to strings to be passed to the preprocessing stage
  * Cannot be used where the query string is const

`qs_scanvalue()`: A slower version that will scan for the given key and will
decode the value into a user-provided string

  * Doesn't alter the query string so can be used where it is const, or cannot be altered
  * Only needs a user-passed char string for copying into
  * Scans the entire qs on each call, so isn't as fast as qs_k2v()


Since `qs_parse()`/`qs_k2v()` alters the query string that is passed to it in a way
that defeats `qs_scanvalue()`, do not mix these two methods (or if you must,
either don't call `qs_scanvalue()` after a call to `qs_parse()` or be sure make
a copy of the query string beforehand)

[note: speed comparisons will be more relevant when the sorting the k/v pairs
code is implemented]


## Installation ##
All you really need is qs_parse.h and qs_parse.c.  I've included my test program
("qs_test.c") and an example program that shows how to use these functions
("qs_example.c").  Also included is a quick Makefile.  If you want to see it
come to life just get all the files and:

    # make
    # ./qs_example


## Bugs, etc. ##

Please let me know if you find any, or if you have license-friendly enhancements
to add.


## License ##

MIT License.  See ./LICENSE or <http://www.opensource.org/licenses/mit-license.php>

Few things are more enjoyable than the knowledge that you've helped another
person. If you do use these functions for anything, I'd love to hear about it:

<bart@bartgrantham.com>


Enjoy!

-Bart
