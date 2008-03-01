#!/usr/bin/env python
import gzip, string, cStringIO, os, time, BaseHTTPServer

data = [str(i) for i in xrange(34000)]
text = "\n".join(data)
s = cStringIO.StringIO()
gz = gzip.GzipFile("1.gz", "wb", 9, s)
gz.write(text)
gz.close()
comp = s.getvalue()
s.close()
pocz = comp[:65536]
reszta = comp[65536:]

class obsluga(BaseHTTPServer.BaseHTTPRequestHandler):
	def do_GET(self):
		self.send_response(200)
		self.send_header("Content-Type", "text/plain")
		self.send_header("Content-Encoding", "gzip")
		self.send_header("Connection", "close")
		self.end_headers()
		self.wfile.write(pocz)
		time.sleep(5)
		self.wfile.write(reszta)

def run(port=8900):
	server_address = ('127.0.0.1', port)
	httpd = BaseHTTPServer.HTTPServer(server_address, obsluga)
	httpd.handle_request()

if __name__ == "__main__":
	run()
