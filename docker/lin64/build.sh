#!/bin/bash
docker build --build-arg NOCACHE=0 -t "elinks-lin64-dev:latest" .
