#!/bin/bash

DEFPROCS=16 # default number of processors to run
PROCS=${1:-$DEFPROCS}  # set processors number to $1 if set, to DEFPROCS otherwise

g++ -o buildmake bmake/buildmake.cc && ./buildmake > paramake
rm altro-results/*.sca
nice make -k -j$PROCS -f paramake
