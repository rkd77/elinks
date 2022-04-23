#!/bin/bash
#
# generate certificate for elinks testing python3 server
#
# just execute and the certificate will be stored
# in /tmp/eltmp.pem
#

openssl req \
  -new \
  -x509 \
  -keyout /tmp/eltmp.pem \
  -out /tmp/eltmp.pem \
  -days 365 \
  -nodes \
  -passout pass:"" \
  -config <(echo '[req]';
    echo distinguished_name=req;
    echo '[san]';
    echo subjectAltName=DNS:localhost,IP:127.0.0.1
    ) \
  -subj '/CN=localhost'
