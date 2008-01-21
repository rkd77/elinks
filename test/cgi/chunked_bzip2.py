#!/usr/bin/env python
import bz2, os

data1 = '<html><body>Two lines should be visible.<br/>'
data2 = 'The second line.</body></html>'

f1 = bz2.BZ2File("/tmp/1.bz2", mode = "wb")
f1.write(data1)
f1.close()
f2 = bz2.BZ2File("/tmp/2.bz2", mode = "wb")
f2.write(data2)
f2.close()

f = open("/tmp/1.bz2")
cd1 = f.read()
f.close()
f3 = open("/tmp/2.bz2")
cd2 = f3.read()
f3.close()

os.unlink("/tmp/1.bz2")
os.unlink("/tmp/2.bz2")

calosc = cd1 + cd2
length = len(calosc)
how_many = 40

len1 = hex(how_many)[2:]
len2 = hex(length - how_many)[2:]

os.write(1, "HTTP/1.0 200 OK\r\nDate: Sun, 20 Jan 2008 15:24:00 GMT\r\nServer: ddd\r\nTransfer-Encoding: chunked\r\nContent-Encoding: bzip2\r\nConnection: close\r\nContent-Type: text/html; charset=ISO-8859-1\r\n\r\n")
os.write(1, "%s\r\n" % len1)
os.write(1, calosc[:how_many])
os.write(1, "\r\n%s\r\n" % len2)
os.write(1, calosc[how_many:])
os.write(1, "\r\n0\r\n\r\n")
