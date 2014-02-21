#!/bin/bash

./fcs_client -F -o localhost | \
	feedgnuplot --lines  \
	--stream 0.1 \
	--xlen 1000 \
	--title 'Monitoring Position Data' \
	--ylabel 'Position Data [arb. units]' \
	--xlabel 'Samples' \
	--legend 0 'X' \
	--legend 1 'Y' \
	--legend 2 'Q' \
	--legend 3 'Sum'

