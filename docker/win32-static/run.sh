#!/bin/bash
sudo docker run -it \
  --name=elinks-win32-static-dev \
  -v /tmp:/tmp/host \
  elinks-win32-static-dev:latest \
  /bin/bash
