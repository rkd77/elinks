#!/bin/bash

trap exit SIGTERM SIGINT

while true
do
	socat -u PIPE:$HOME/.elinks/clipboard.fifo EXEC:"xclip -sel clip"
done
