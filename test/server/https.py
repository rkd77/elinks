#!/usr/bin/python3
#
# testing server for elinks https
#
# Note: Don't forget to generate the certificate using
# gen.sh
#
# if You provide the directory it serves it. The idea
# is to be run in the elinks test directory to provide
# test files.
#
# Note: has to be run from the test directory e.g. like this:
#
# $ ./server/https.py index.html
#

PORT = 9453
CERTFILE='/tmp/eltmp.pem'

import ssl
import http.server
import socketserver

handler = http.server.SimpleHTTPRequestHandler

with socketserver.TCPServer(('127.0.0.1', PORT), handler) as httpd:
  print("[*] https server started at localhost:" + str(PORT))
  httpd.socket = ssl.wrap_socket(
    httpd.socket,
    certfile=CERTFILE,
    server_side=True
  )
  httpd.serve_forever()

if __name__ == "__main__":
  run()
