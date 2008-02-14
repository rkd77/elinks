#!/usr/bin/env python
import gzip, os, time, StringIO

output = StringIO.StringIO()

data1 = '<html><body>Two lines should be visible.<br/>The second line.</body></html>'

f1 = gzip.GzipFile("/tmp/1.gz", mode = "wb", fileobj=output)
f1.write(data1)
f1.close()

cd1 = output.getvalue()
output.close()

length = len(cd1)
next_chunk = hex(length - 10)[2:]

os.write(1, "Date: Sun, 20 Jan 2008 15:24:00 GMT\r\nServer: ddd\r\nTransfer-Encoding: chunked\r\nContent-Encoding: gzip\r\nConnection: close\r\nContent-Type: text/html; charset=ISO-8859-1\r\n")
os.write(1, "\r\na\r\n")
os.write(1, cd1[:10])
time.sleep(2)
os.write(1, "\r\n%s\r\n" % next_chunk)
os.write(1, cd1[10:])
os.write(1, "\r\n0\r\n")
