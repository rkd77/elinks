#!/usr/bin/python3

import subprocess
import sys

if __name__ == '__main__':
        args = ['/usr/bin/i586-pc-msdosdjgpp-gcc']
        for i in sys.argv[1:]:
                if i == '-rdynamic':
                        continue
                args.append(i)
        sys.exit(subprocess.call(args))
