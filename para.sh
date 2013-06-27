#!/bin/bash

DEFPROCS=80 # default number of processors to run
PROCS=${1:-$DEFPROCS}  # set processors number to $1 if set, to DEFPROCS otherwise

g++ -o buildmake bmake/buildmake.cc && ./buildmake > paramake
time nice make -k -j$PROCS -f paramake 2>&1 | grep -i error > errlog-`date +%F`.txt
tail errlog-`date +%F`.txt
