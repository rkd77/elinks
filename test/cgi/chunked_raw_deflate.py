#!/usr/bin/env python
import os, time
from zlib import *

# According to section 3.5 of RFC 2616, "Content-Encoding: deflate"
# requires a ZLIB header.  However, Microsoft-IIS/6.0 sends a raw
# DEFLATE stream instead.  This CGI tests how ELinks handles that.

data1 = '<html><body>Two lines should be visible.<br/>The second line.</body></html>'
ob = compressobj(Z_DEFAULT_COMPRESSION, DEFLATED, -MAX_WBITS)
cd1 = ob.compress(data1)
cd1 += ob.flush()
length = len(cd1)
next_chunk = hex(length - 10)[2:]

os.write(1, "Date: Sun, 20 Jan 2008 15:24:00 GMT\r\nServer: ddd\r\nTransfer-Encoding: chunked\r\nContent-Encoding: deflate\r\nConnection: close\r\nContent-Type: text/html; charset=ISO-8859-1\r\n")
os.write(1, "\r\na\r\n")
os.write(1, cd1[:10])
time.sleep(2)
os.write(1, "\r\n%s\r\n" % next_chunk)
os.write(1, cd1[10:])
os.write(1, "\r\n0\r\n")
