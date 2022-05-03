#!/bin/bash
docker run -it \
  --name=elinks \
  -v /tmp:/tmp/host \
  elinks-dev:latest \
  /bin/bash
