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
fcs_client -l $nsamples -c 0 -o localhost 
# Start Acquisistion
fcs_client -t -o localhost 
# Retrieve Samples
fcs_client -B 0 -o localhost | \
	feedgnuplot --lines \
	--title 'ADC Data' \
	--ylabel 'ADC counts [arb. units]' \
	--xlabel 'Samples' \
	--legend 0 'A' \
	--legend 1 'B' \
	--legend 2 'C' \
	--legend 3 'D'
