#!/bin/bash

./fcs_client -F -o localhost | feedgnuplot --lines --stream 0.1 --xlen 1000 --ylabel 'Position data' --xlabel second
