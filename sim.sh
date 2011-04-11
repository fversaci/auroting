#! /bin/sh
nice opp_runall ./aurouting -u Cmdenv -c bub -r 0..$1 &
Sim1=$!
nice opp_runall ./aurouting -u Cmdenv -c tb  -r 0..$1 &
Sim2=$!
wait $Sim1
wait $Sim2
