#!/bin/bash
sudo docker run -it \
  --name=elinks-djgpp-dev \
  -v /tmp:/tmp/host \
  elinks-djgpp-dev:latest \
  /bin/bash
