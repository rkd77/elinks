#!/bin/bash
#
# shell script to test elinks binary
#

clear

echo '       --/                                    \--'
echo '       --[ Welcome to the elinks test helper  ]--'
echo '       --[                                    ]--'
echo '       --[ [*] use option 1 to run dump test  ]--'
echo '       --\                                    /--'
echo '                                                 '

# SET TEST
prep_test() {
  if [ "$1" = "hello" ]; then
    ./src/elinks --dump ./test/hello.html
  fi
}

# MAIN LOOP
prep_test hello
OPTS="hello exit"
select SEL in $OPTS; do
  if [ ! "$SEL" = "exit" ]; then
    prep_test $SEL
  else
    exit
  fi
  echo "--[ Current test : " $SEL" ]--"
done
