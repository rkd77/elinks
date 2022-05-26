#!/bin/bash
docker run -it \
  --name=elinks-lin64-dev \
  -v /tmp:/tmp/host \
  elinks-lin64-dev:latest \
  /bin/bash
