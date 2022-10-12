#!/usr/bin/python3

import os

if __name__ == '__main__':
    podir = os.path.dirname(os.path.realpath(__file__))
    topsrcdir = os.path.abspath(os.path.join(podir, '..'))
    potfiles = os.path.join(podir, 'potfiles.list')
    os.chdir(topsrcdir)
    os.system("find src/ -type f -name '*.[ch]' -o -name '*.cpp' -o -name options.inc -o -name 'actions-*.inc' | sort > %s" % potfiles)
