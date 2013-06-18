#! /bin/sh

tmp=$(tempfile) || exit

for PATT in Bitreverse Butterfly Uniform Transposition TDTrans Bitcomplement; do
    last=`ls $PATT-* | cut -d "-" -f 2 | cut -d "." -f 1 | sort -n | tail -n 1`
    echo $PATT: $last
    step=4
    for alg in `seq 0 $((step-1))`; do
	#echo "$PATT Alg $alg, derouted:" >> "$tmp";
	#for n in `seq $alg $step $last`; do
	#    #echo -n "$n: " >> "$tmp";
	#    grep totalDerouted $PATT-$n.sca | cut -c 43- | tr -d '\n' >> "$tmp";
	#    echo -n ", " >> "$tmp";
	#done
	#echo "\n" >> "$tmp";
	##########################
	##########################
	echo "$PATT Alg $alg:, lifetime" >> "$tmp";
	for n in `seq $alg $step $last`; do
	    #echo -n "$n: " >> "$tmp";
	    #grep mean $PATT-$n.sca | head -n 1 | cut -c 12- | tr -d '\n' >> "$tmp";
	    grep mean $PATT-$n.sca | tail -n 1 | cut -c 12- | tr -d '\n' >> "$tmp";
	    echo -n ", " >> "$tmp";
	done
	echo "\n" >> "$tmp";
    done
done

cp "$tmp" plots.txt
rm "$tmp"
