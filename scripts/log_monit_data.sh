#!/bin/bash

TODAY=$(date --iso-8601)
LOGFILE_DIR=~
MONIT_AMP_LOG=monit_amp
MONIT_POS_LOG=monit_pos

fcs_client -E -O -o localhost > ${LOGFILE_DIR}/${MONIT_AMP_LOG}_${TODAY}.txt &
fcs_client -F -O -o localhost > ${LOGFILE_DIR}/${MONIT_POS_LOG}_${TODAY}.txt &
