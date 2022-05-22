#!/bin/bash
docker run -it \
  --name=elinks-arm32-dev \
  -v /tmp:/tmp/host \
  elinks-arm32-dev:latest \
  /bin/bash
