#!/bin/bash
docker run -it \
  --name=elinks-win32-dev \
  -v /tmp:/tmp/host \
  elinks-win32-dev:latest \
  /bin/bash
