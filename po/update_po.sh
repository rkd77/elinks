#!/bin/bash

SRC=$(dirname -- ${BASH_SOURCE[0]})

for po in "$SRC"/*.po
do
	lang=$(basename $po .po)
	echo -n "$lang"
	if $(msgmerge "$SRC/$lang.po" "$SRC/elinks.pot" -o "$lang.new.po"); then
		mv -f "$lang.new.po" "$SRC/$lang.po"
	else
		echo "msgmerge failed!"
		rm -f "$lang.new.po"
	fi
done
