#!/usr/bin/python3

import os
import sys

if __name__ == '__main__':
    was_elinks = False
    with os.popen('wine {0} {1}'.format(os.getenv('ELINKS_BINARY'), sys.argv[1]), 'r') as fi:
        for line in fi:
            if not was_elinks:
                if not line.startswith('ELinks'):
                   continue
                was_elinks = True
            print(line, end='')
    sys.exit(0)
