#!/usr/bin/env python
import os, zlib

data1 = '<html><body>Two lines should be visible.<br/>'
data2 = 'The second line.</body></html>'

cd1 = zlib.compress(data1)
cd2 = zlib.compress(data2)

calosc = cd1 + cd2
length = len(calosc)

how_many = 40
len1 = hex(how_many)[2:]
len2 = hex(length - how_many)[2:]

os.write(1, "HTTP/1.0 200 OK\r\nDate: Sun, 20 Jan 2008 15:24:00 GMT\r\nServer: ddd\r\nTransfer-Encoding: chunked\r\nContent-Encoding: deflate\r\nConnection: close\r\nContent-Type: text/html; charset=ISO-8859-1\r\n\r\n")
os.write(1, "%s\r\n" % len1)
os.write(1, calosc[:how_many])
os.write(1, "\r\n%s\r\n" % len2)
os.write(1, calosc[how_many:])
os.write(1, "\r\n0\r\n\r\n")
