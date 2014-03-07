#!/bin/bash

# Number of samples
EXPECTED_ARGS=1

if [ $# -ne $EXPECTED_ARGS ]
then
	echo "Usage: `basename $0` {number of samples}"
	exit 1;
fi

nsamples=$1

./get_adc_data.sh $nsamples | \
	feedgnuplot --lines \
	--title 'ADC Data' \
	--ylabel 'ADC counts [arb. units]' \
	--xlabel 'Samples' \
	--legend 0 'A' \
	--legend 1 'B' \
	--legend 2 'C' \
	--legend 3 'D'
