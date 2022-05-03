#!/bin/bash
docker run -it \
  --name=elinks-win32-dev \
  -v /home/miky:/home/miky \
  elinks-win32-dev:latest \
  /bin/bash
