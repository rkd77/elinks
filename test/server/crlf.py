#!/usr/bin/env python

import os
from BaseHTTPServer import *
import tempfile
import signal

r, w = os.pipe()

C_CR = 0
C_LF = 1
C_CRLF = 2

E_Raw = 0
E_Entity = 1
E_JavaScript = 2

F_Hidden = 0
F_TextArea = 1

def encode(ch, encoding):
	if ch == C_CRLF:
		return encode(C_CR, encoding) + encode(C_LF, encoding)
	if ch == C_CR:
		if encoding == E_Raw:
			return "\r"
		if encoding == E_JavaScript:
			return "\\r"
		if encoding == E_Entity:
			return "&#013;"
	if ch == C_LF:
		if encoding == E_Raw:
			return "\n"
		if encoding == E_JavaScript:
			return "\\n"
		if encoding == E_Entity:
			return "&#010;"

def get_form(ch, encoding, field):
	text = "foo" + encode(ch, encoding) + "bar"
	if encoding == E_JavaScript:
		text_initial = ""
	else:
		text_initial = text

	s = """<html>
<head>
<title>Form Test</title>
</head>
<body>
<form id="form1" name="form1" action="http://127.0.0.1:8090/">
"""
	if field == F_Hidden:
		s += '<input type="hidden" id="field1" name="field1" value="' + text_initial + '">'
	elif field == F_TextArea:
		s += '<textarea id="field1" name="field1">' + text_initial + '</textarea>'
	s += "\n</form>"
	if encoding == E_JavaScript:
		s += """
<script>
document.form1.field1.value = '%s';
</script>""" % (text)

	s += "</body></html>"
	return s

class forwarder(BaseHTTPRequestHandler):
	def do_GET(self):
		w.write(self.path + "\n")
		w.flush()
		self.send_response(200)
		self.send_header("Content-Type", "text/plain")
		self.end_headers()
		self.wfile.write("Dummy response")

def runtest(r, *args):
	form = get_form(*args)

	tmpfile, tmpname = tempfile.mkstemp(".html")
	tmpfile = os.fdopen(tmpfile, 'w')
	tmpfile.write(form)
	tmpfile.close()

	linkspid = os.spawnlp(os.P_NOWAIT, 'elinks', 'elinks',
		'-config-dir', os.getcwd(),
		'-config-file', 'crlf.conf',
		'-no-connect', '1',
		'-auto-submit', '1',
		tmpname)
	path = r.readline()
	os.kill(linkspid, signal.SIGINT)
	os.waitpid(linkspid, 0)

	os.unlink(tmpname)

	return path

pid = os.fork()

if pid:
	os.close(w)
	r = os.fdopen(r)

	paths = []

	for c in [C_CR, C_LF, C_CRLF]:
		for e in [E_Raw, E_Entity, E_JavaScript]:
			for f in [F_Hidden, F_TextArea]:
				paths.append(("%d %d %d " % (c, e, f)) + runtest(r, c, e, f))

	for path in paths:
		print path,

	os.kill(pid, signal.SIGTERM)
	os.waitpid(pid, 0)
else:
	os.close(r)
	w = os.fdopen(w, 'w')
	server_address = ('127.0.0.1', 8090)
	httpd = HTTPServer(server_address, forwarder)
	httpd.serve_forever()
