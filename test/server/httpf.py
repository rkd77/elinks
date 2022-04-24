#!/usr/bin/python3
#
# testing server for elinks http
#

PORT = 9452

import http.server
import socketserver

handler = http.server.SimpleHTTPRequestHandler

with socketserver.TCPServer(('127.0.0.1', PORT), handler) as httpd:
  print("[*] http server started at localhost:" + str(PORT))
  httpd.serve_forever()
