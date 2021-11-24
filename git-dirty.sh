#!/bin/sh

cd "$MESON_SOURCE_ROOT"

for line in $(git diff-index HEAD 2>/dev/null)
do
    echo '-dirty'
    exit 0
done
