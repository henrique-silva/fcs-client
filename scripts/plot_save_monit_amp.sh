#!/bin/bash

# Filename
EXPECTED_ARGS=1

if [ $# -ne $EXPECTED_ARGS ]
then
	echo "Usage: `basename $0` {filename}"
	exit 1;
fi

filename=$1

fcs_client -E -O -o localhost | \
	tee -a ${filename} | \
	awk '{print $2, $3, $4, $5; system("")}' | \
	feedgnuplot --lines  \
	--stream 0.1 \
	--xlen 1000 \
	--title 'Monitoring Amplitude Data' \
	--ylabel 'Amplitude Data [arb. units]' \
	--xlabel 'Samples' \
	--legend 0 'A' \
	--legend 1 'B' \
	--legend 2 'C' \
	--legend 3 'D'

