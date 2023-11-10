#!/bin/bash
sudo docker run -it \
  --name=elinks-win64-static-dev \
  -v /tmp:/tmp/host \
  elinks-win64-static-dev:latest \
  /bin/bash
