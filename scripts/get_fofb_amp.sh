#!/bin/bash

# Number of samples
EXPECTED_ARGS=1

if [ $# -ne $EXPECTED_ARGS ]
then
	echo "Usage: `basename $0` {number of samples}"
	exit 1;
fi

nsamples=$1

# Set acquisition parameters
fcs_client -l $nsamples -c 3 -o localhost 
# Start Acquisistion
fcs_client -t -o localhost 
# Retrieve Samples
fcs_client -B 3 -o localhost
