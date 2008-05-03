#!/usr/bin/env python
import md5
import cgi

print "Content-Type: text/plain\r\n"
form = cgi.FieldStorage()
if form.has_key("file"):
	plik = form["file"]
	length = 0
	if plik.file:
		dig = md5.new()
		while 1:
			data = plik.file.read(1000000)
			if not data:
				break
			length += len(data)
			dig.update(data)

		print "Filename = " + plik.filename
		print "Size = %d" % length
		print "MD5 = " + dig.hexdigest()
