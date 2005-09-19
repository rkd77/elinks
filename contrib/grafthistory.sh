#!/bin/sh
#
# Graft the ELinks development history to the current tree.
#
# Note that this will download about 80M.

if [ -z "`which wget 2>/dev/null`" ]; then
  echo "Error: You need to have wget installed so that I can fetch the history." >&2
  exit 1
fi

[ -d .git ] && cd .git

echo "[grafthistory] Downloading the history"
cd objects/pack
wget -c http://elinks.or.cz/elinks-history.git/objects/pack/pack-0d6c5c67aab3b9d5d9b245da5929c15d79124a48.idx
wget -c http://elinks.or.cz/elinks-history.git/objects/pack/pack-0d6c5c67aab3b9d5d9b245da5929c15d79124a48.pack

echo "[grafthistory] Setting up the grafts"
cd ../..
# master
echo 0f6d4310ad37550be3323fab80456e4953698bf0 06135dc2b8bb7ed2e441305bdaa82048396de633 >>info/grafts
# REL_0_10
echo 43a9a406737fd22a8558c47c74b4ad04d4c92a2b 730242dcf2cdeed13eae7e8b0c5f47bb03326792 >>info/grafts

echo "[grafthistory] Refreshing the dumb server info wrt. new packs"
cd ..
git-update-server-info
