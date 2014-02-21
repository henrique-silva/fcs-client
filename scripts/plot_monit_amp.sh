#!/bin/bash

./fcs_client -E -o localhost | \
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

