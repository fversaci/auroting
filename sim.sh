#! /bin/sh
rm results/*

max=$(($1-1))
nice opp_runall ./aurouting -u Cmdenv -c bub -r 0..$max &
Sim1=$!
nice opp_runall ./aurouting -u Cmdenv -c tb  -r 0..$max &
Sim2=$!
wait $Sim1
wait $Sim2
