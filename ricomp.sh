#!/bin/bash
make clean && opp_makemake -f && make -j 7 MODE=release CONFIGNAME=gcc-release all
