#! /bin/sh
# ELinks old bookmarks format to new format converter.

# WARNING: Close all ELinks sessions before running this script.
# This script converts ELinks bookmarks file with '|' as separator to new
# bookmarks format where separator is tab char. It saves old file to
# ~/.links/bookmarks.with_pipes. --Zas

# Script by Stephane Chazelas :)

BMFILE=$HOME/.links/bookmarks
if [ ! -r "$BMFILE" ]; then
	echo "$BMFILE does not exist or is not readable!" >&2
	exit 1
fi

if [ -f "${BMFILE}.with_pipes" ]; then
	echo "It seems you already ran this script." >&2
	echo "Remove ${BMFILE}.with_pipes to force execution." >&2
	exit 1
fi
 
if cp -f "$BMFILE" "${BMFILE}.with_pipes" \
     && tr '|' '\011' < ${BMFILE}.with_pipes > $BMFILE
then
  echo "Bookmarks file converted."
  echo "Old file was saved as ${BMFILE}.with_pipes."
  echo "You may want to copy ~/.links/bookmarks to ~/.elinks/bookmarks now."
else
  echo "Conversion failure" >&2
  exit 1
fi
