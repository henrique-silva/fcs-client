#!/bin/bash

# Number of samples
EXPECTED_ARGS=1

if [ $# -ne $EXPECTED_ARGS ]
then
	echo "Usage: `basename $0` {number of samples}"
	exit 1;
fi

nsamples=$1

./get_fofb_pos.sh $nsamples | \
	feedgnuplot --lines \
	--title 'FOFB Position Data' \
	--ylabel 'Position data [nm]' \
	--xlabel 'Samples' \
	--legend 0 'X' \
	--legend 1 'Y' \
	--legend 2 'Q' \
	--legend 3 'Sum'
