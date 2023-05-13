#!/bin/bash

trap exit SIGTERM SIGINT

while true
do
	socat -u PIPE:$HOME/.config/elinks/clipboard.fifo EXEC:"wl-copy"
done
