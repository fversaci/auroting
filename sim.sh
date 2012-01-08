#!/bin/bash

DEFPROCS=2 # default number of processors to run
PROCS=${1:-$DEFPROCS}  # set processors number to $1 if set, to DEFPROCS otherwise

##CONFS=(Uniform Butterfly Transposition)


#CONFS[${#CONFS[*]}]='Uniform'
#CONFS[${#CONFS[*]}]='Butterfly'
#CONFS[${#CONFS[*]}]='Transposition'
#CONFS[${#CONFS[*]}]='Permutation'
#CONFS[${#CONFS[*]}]='Bitcomplement'
#CONFS[${#CONFS[*]}]='Tornado'
#CONFS[${#CONFS[*]}]='Local'
CONFS[${#CONFS[*]}]='Test'


for CO in ${CONFS[*]}; do

  # delete old simulations files
    find results/ \( -name "$CO*.sca" -o -name "$CO*.vec" -o -name "$CO-.vci" \) -delete
  # run new simulation and wait until completion
    max=`./aurouting -u Cmdenv -x $CO | grep 'Number of runs:' | cut -c 17-`
    max=$(($max-1))
    nice opp_runall -j$PROCS ./aurouting -u Cmdenv -c $CO -r 0..$max 

done

