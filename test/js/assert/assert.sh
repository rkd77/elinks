#!/bin/sh

d=$(dirname "$0")

for i in $d/*.html; do
	readlink -f "$i"
done
