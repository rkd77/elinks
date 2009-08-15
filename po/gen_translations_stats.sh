#!/bin/bash

# This script prints translations statistics for .po files
# existing in the current directory

echo "Translations statistics"
echo "Date: `date`"
echo

for i in *.po; do
	msgfmt --statistics -o /dev/null $i 2>&1 \
	| sed 's/^\([0-9]\+ \)[^0-9]*\([0-9]\+ \)\?[^0-9]*\([0-9]\+ \)\?[^0-9]*$/\1\2\3/g' \
	| awk '{ \
		tot = $1 + $2 + $3; \
		if (tot != 0) \
			printf "%8.0f %8s %6.02f%% (%3d/%3d untranslated)\n",\
			($1*100/tot)*100, "'"$i"'", $1*100/tot, $2+$3, tot}' ;
done | sort -b -k1,1nr -k2,2 | sed 's/^ *[0-9]*//'

echo

