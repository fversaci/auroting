#! /bin/sh

DEFPROCS=2 # default number of processors to run
PROCS={1:-DEFPROCS}  # set processors number to $1 if set, to DEFPROCS otherwise

# delete old simulations files
find results/ \( -name '*.sca' -o -name '*.vec' -o -name '*.vci' \) -delete

# run new simulation and wait until completion
max=`./aurouting -u Cmdenv -x bub | grep 'Number of runs:' | cut -c 17-`
max=$(($max-1))
nice opp_runall -j$PROCS ./aurouting -u Cmdenv -c bub -r 0..$max &
Sim1=$!

#nice opp_runall ./aurouting -u Cmdenv -c tb  -r 0..$max &
#Sim2=$!
#wait $Sim2

wait $Sim1
