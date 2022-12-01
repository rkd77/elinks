#!/bin/bash
sudo docker run -it \
  --name=elinks-alpine-static-dev \
  -v /tmp:/tmp/host \
  elinks-alpine-static-dev:latest \
  /bin/sh
