#!/usr/bin/env python
import os, time, zlib

data1 = '<html><body>Two lines should be visible.<br/>The second line.</body></html>'
cd1 = zlib.compress(data1)

length = len(cd1)
next_chunk = hex(length - 10)[2:]

os.write(1, "Date: Sun, 20 Jan 2008 15:24:00 GMT\r\nServer: ddd\r\nTransfer-Encoding: chunked\r\nContent-Encoding: deflate\r\nConnection: close\r\nContent-Type: text/html; charset=ISO-8859-1\r\n")
os.write(1, "\r\na\r\n")
os.write(1, cd1[:10])
time.sleep(2)
os.write(1, "\r\n%s\r\n" % next_chunk)
os.write(1, cd1[10:])
os.write(1, "\r\n0\r\n")
