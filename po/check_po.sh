#!/bin/bash

SRC=$(dirname -- ${BASH_SOURCE[0]})

for po in "$SRC"/*.po
do
	lang=$(basename $po .po)
	echo -n "$lang"
	msgfmt --check --check-accelerators="~" --verbose --statistics -o /dev/null "$SRC/$lang.po"
	perl -I"$SRC/perl" "$SRC/perl/msgaccel-check" "$SRC/$lang.po"
done
