#!/bin/sh -e
#
# Graft the ELinks development history to the current tree.
#
# Note that this will download about 80M.

if [ -n "`which wget 2>/dev/null`" ]; then
  downloader="wget -c"
elif [ -n "`which curl 2>/dev/null`" ]; then
  downloader="curl -C - -O"
else
  echo "Error: You need to have wget or curl installed so that I can fetch the history." >&2
  exit 1
fi

[ "$GIT_DIR" ] || GIT_DIR=.git
if ! [ -d "$GIT_DIR" ]; then
  echo "Error: You must run this from the project root (or set GIT_DIR to your .git directory)." >&2
  exit 1
fi
cd "$GIT_DIR"

echo "[grafthistory] Downloading the history"
mkdir -p objects/pack
cd objects/pack
echo "ELinks history converted from CVS.  Keep this pack separate to speed up git gc." \
     > pack-0d6c5c67aab3b9d5d9b245da5929c15d79124a48.keep
# pack-0d6c5c67aab3b9d5d9b245da5929c15d79124a48.idx is 3163784 bytes long.
# Downloading it takes less than 6 seconds here, whereas generating it
# with git index-pack takes over 4 minutes (750 MHz Duron, git 1.5.4.1).
$downloader http://elinks.cz/elinks-history.git/objects/pack/pack-0d6c5c67aab3b9d5d9b245da5929c15d79124a48.idx
$downloader http://elinks.cz/elinks-history.git/objects/pack/pack-0d6c5c67aab3b9d5d9b245da5929c15d79124a48.pack

echo "[grafthistory] Setting up the grafts"
cd ../..
mkdir -p info
# master
echo 0f6d4310ad37550be3323fab80456e4953698bf0 06135dc2b8bb7ed2e441305bdaa82048396de633 >>info/grafts
# REL_0_10
echo 43a9a406737fd22a8558c47c74b4ad04d4c92a2b 730242dcf2cdeed13eae7e8b0c5f47bb03326792 >>info/grafts

echo "[grafthistory] Refreshing the dumb server info wrt. new packs"
cd ..
git update-server-info
