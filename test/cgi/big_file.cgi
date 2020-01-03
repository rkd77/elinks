#!/usr/bin/env python3

import hashlib
import cgi

print("Content-Type: text/plain\r\n")
form = cgi.FieldStorage()
if "file" in form:
    plik = form["file"]
    length = 0
    if plik.file:
        dig = hashlib.md5()
        while 1:
            data = plik.file.read(1000000)
            if not data:
                break
            length += len(data)
            dig.update(data)

        print("Filename2 = " + plik.filename)
        print("Size = %d" % length)
        print("MD5 = " + dig.hexdigest())
