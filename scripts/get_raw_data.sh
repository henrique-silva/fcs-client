#!/bin/bash

# Number of samples
EXPECTED_ARGS=2

if [ $# -ne $EXPECTED_ARGS ]
then
	echo "Usage: `basename $0` {number of samples} {channel number}"
	exit 1;
fi

nsamples=$1
chan=$2

# Set acquisition parameters, start acquisition and retrieve samples
fcs_client -l $nsamples -c $chan -t -B $chan -o localhost 
