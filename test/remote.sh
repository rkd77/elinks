#!/bin/bash
#
# Test various -remote use cases. Start an ELinks instance before
# running the test. All arguments to this test script will be passed to
# ELinks. Set ELINKS to change the binary to execute ELinks.
#
# XXX: The test will affect your current ELinks configurations. If you
# do not want this pass -config-dir <path/to/dir/> to avoid this.
#
# FIXME: Maybe make this script eun more automatic by using screen.
#
# EDIT: There is now -session-ring <num> and -base-session <num>
# used to prevent intererence with existing running elinks
#
# DOC: For this test GNU screen is recommended. If I assume You'll
# be in the elinks sources directory and there isn't and elinks
# currently running (ps axf | grep elinks). Then use:
#
# $ screen -S test
#
# then ctrl+a and c - create new screen and create new elinks instance
#
# $ ./src/elinks
#
# then return to the original window using ctrl + a and a
# and execute the elinks remote control tests:
#
# $ ./test/remote.sh
#
# If everything is well the remote commands should be executed in the
# running elinks session and You'll see the results if You press:
# ctrl+a and a
# 

die()
{
	echo "$@" >&2
	exit 1
}

test_remote()
{
        #echo $args
        #echo $1
	desc="$1"; shift
	testno=$(expr "$testno" + 1)
        echo "[*] test $testno: $desc "
	#echo "	> $elinks $confdir --remote '$@'"
	$elinks $args --remote "$@"
	#echo "Press return to continue..."
	#read
}

# MAIN SETUP
# elinks binary
elinks="`pwd`/src/elinks"
#elinks="$elinks -config-dir /home/`whoami`/.elinks"
# custom args
args="$@"
# command to start new elinks session ring
testno=100
# go to current directory in the shell script
cd `pwd`

# MAIN PROGRAM
echo "[=] Starting remote testing: " $elinks

# tests 010 if remote is working
$elinks $args --remote "ping()" || die "Start ELinks instance to proceed"

# tests 020 infoBoxes
test_remote "infoBox(): no quote" "infoBox(Hello World.)"
test_remote "infoBox(): quote" 'infoBox("Hello World.")'
test_remote "infoBox(): single quote (not considered as quote chars)" "infoBox('Hello World.')"
test_remote "infoBox(): quoted quote" 'infoBox("Hello ""quoted"" World.")'
test_remote "infoBox(): quoted string with comma" 'infoBox("Comma, a different kind of punctuation.")'

# tests 030  open url
test_remote "openURL(): prompt URL" "openURL()"
test_remote "openURL(): in current tab" 'openURL("http://elinks.cz/")'
test_remote "openURL(): in new tab" 'openURL(http://elinks.cz/news.html, new-tab)'
test_remote "openURL(): in new tab" 'openURL("http://elinks.cz/news.html", "new-tab")'
test_remote "openURL(): in new tab" 'openURL("http://elinks.cz/news.html", "new-tab")'
test_remote "openURL(): in new window (requires that ELinks runs in screen or a window environment)" \
	    "openURL(http://elinks.cz/search.html, new-window)"


# tests 040  open new window
test_remote "xfeDoCommand(): open new window (requires that ELinks runs in screen or a window environment)" \
	    "xfeDoCommand(openBrowser)"

# tests 050 add bookmark
test_remote "addBookmark()" 'addBookmark("http://127.0.0.1/")'

# tests 060 single url
test_remote "ELinks extension: single URL" "/"
