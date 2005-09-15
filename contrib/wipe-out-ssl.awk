#!/bin/awk -f
BEGIN {
	iflevel = 0;
}

{
	if (!iflevel) {
		if ($1 == "#ifdef" && $2 == "HAVE_SSL") iflevel = 1;
		else print $0;
	} else {
		if (iflevel == 1 && ($1 == "#else" || $1 == "#elif")) {
			iflevel = 0;
			if ($1 == "#else") print "#if 1";
			else {
				print "#if 0";
				print $0;
			}
		}
		if ($1 == "#if" || $1 == "#ifdef" || $1 == "#ifndef") iflevel++;
		if ($1 == "#endif") iflevel--;
	}
}
