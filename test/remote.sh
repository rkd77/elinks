#!/bin/sh
#
# Test various -remote use cases. Start an ELinks instance before
# running the test. All arguments to this test script will be passed to
# ELinks. Set ELINKS to change the binary to execute ELinks.
#
# XXX: The test will affect your current ELinks configurations. If you
# do not want this pass --config-dir <path/to/dir/> to avoid this.
# 
# FIXME: Maybe make this script eun more automatic by using screen. 

elinks=${ELINKS:-elinks}
args="$@"

die()
{
	echo "$@" >&2
	exit 1
}

test_remote()
{
	desc="$1"; shift
	testno=$(expr "$testno" + 1)
	echo "Test $testno: $desc"
	echo "	> $elinks $confdir --remote '$@'"
	"$elinks" $args --remote "$@"
	echo "Press return to continue..."
	read
}

elinks=${ELINKS:-elinks}
testno=0

"$elinks" $args --remote "ping()" || die "Start ELinks instance to proceed"

test_remote "infoBox(): no quote" "infoBox(Hello World.)"
test_remote "infoBox(): quote" 'infoBox("Hello World.")'
test_remote "infoBox(): single quote (not considered as quote chars)" "infoBox('Hello World.')"
test_remote "infoBox(): quoted quote" 'infoBox("Hello ""quoted"" World.")'
test_remote "infoBox(): quoted string with comma" 'infoBox("Comma, a different kind of punctuation.")'

test_remote "openURL(): prompt URL" "openURL()"
test_remote "openURL(): in current tab" 'openURL("http://elinks.cz/")'
test_remote "openURL(): in new tab" 'openURL(http://elinks.cz/news.html, new-tab)'
test_remote "openURL(): in new tab" 'openURL("http://elinks.cz/news.html", "new-tab")'
test_remote "openURL(): in new tab" 'openURL("http://elinks.cz/news.html", "new-tab")'
test_remote "openURL(): in new window (requires that ELinks runs in screen or a window environment)" \
	    "openURL(http://elinks.cz/search.html, new-window)"

test_remote "xfeDoCommand(): open new window (requires that ELinks runs in screen or a window environment)" \
	    "xfeDoCommand(openBrowser)"

test_remote "addBookmark()" 'addBookmark("http://127.0.0.1/")'

test_remote "ELinks extension: single URL" "/"
