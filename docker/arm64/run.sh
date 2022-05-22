#!/bin/bash
docker run -it \
  --name=elinks-arm64-dev \
  -v /tmp:/tmp/host \
  elinks-arm64-dev:latest \
  /bin/bash
