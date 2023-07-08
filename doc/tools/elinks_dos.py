#!/usr/bin/python3

import os
import sys

if __name__ == '__main__':
    was_elinks = False
    d = os.path.dirname(os.getenv('ELINKS_BINARY'))
    with os.popen('dosemu -dumb -K {0} -E "elinks {1}"'.format(d, sys.argv[1]), 'r') as fi:
        for line in fi:
            if not was_elinks:
                if not line.startswith('ELinks'):
                   continue
                was_elinks = True
            print(line, end='')
    sys.exit(0)
