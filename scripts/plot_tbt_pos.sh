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
fcs_client -l $nsamples -c 2 -o localhost 
# Start Acquisistion
fcs_client -t -o localhost 
# Retrieve Samples
fcs_client -B 2 -o localhost | \
	feedgnuplot --lines \
	--title 'TBT Position Data' \
	--ylabel 'Position Data [nm]' \
	--xlabel 'Samples' \
	--legend 0 'X' \
	--legend 1 'Y' \
	--legend 2 'Q' \
	--legend 3 'Sum'
