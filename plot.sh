#! /bin/sh

tmp=$(tempfile) || exit

for PATT in Bitreverse Butterfly Uniform Transposition TDTrans Bitcomplement; do
    last=`ls $PATT-* | cut -d "-" -f 2 | cut -d "." -f 1 | sort -n | tail -n 1`
    echo $PATT: $last
    step=3
    for alg in `seq 0 $((step-1))`; do
	# Total derouted
	echo "$PATT Alg $alg, derouted:" >> "$tmp";
	echo -n "tot: " >> "$tmp";
	for n in `seq $alg $step $last`; do
	    #echo -n "$n: " >> "$tmp";
	    grep totalDerouted $PATT-$n.sca | cut -c 43- | tr -d '\n' >> "$tmp";
	    echo -n ", " >> "$tmp";
	done
	echo -n "\n" >> "$tmp";
	##########################
	# Total outflanked
	#echo "$PATT Alg $alg, outflanked:" >> "$tmp";
	echo -n "ofr: " >> "$tmp";
	for n in `seq $alg $step $last`; do
	    #echo -n "$n: " >> "$tmp";
	    grep totalOutFlanked $PATT-$n.sca | cut -c 45- | tr -d '\n' >> "$tmp";
	    echo -n ", " >> "$tmp";
	done
	echo -n "\n" >> "$tmp";
	##########################
	# Total oidn
	#echo "$PATT Alg $alg, outflanked:" >> "$tmp";
	echo -n "oidn: " >> "$tmp";
	for n in `seq $alg $step $last`; do
	    #echo -n "$n: " >> "$tmp";
	    grep totalOidn $PATT-$n.sca | cut -c 39- | tr -d '\n' >> "$tmp";
	    echo -n ", " >> "$tmp";
	done
	echo -n "\n" >> "$tmp";
	##########################
	# Total widn
	#echo "$PATT Alg $alg, outflanked:" >> "$tmp";
	echo -n "widn: " >> "$tmp";
	for n in `seq $alg $step $last`; do
	    #echo -n "$n: " >> "$tmp";
	    grep totalWidn $PATT-$n.sca | cut -c 39- | tr -d '\n' >> "$tmp";
	    echo -n ", " >> "$tmp";
	done
	echo -n "\n" >> "$tmp";
	##########################
	# Total cqr-ed
	#echo "$PATT Alg $alg, cqr-ed:" >> "$tmp";
	echo -n "cqr: " >> "$tmp";
	for n in `seq $alg $step $last`; do
	    #echo -n "$n: " >> "$tmp";
	    grep totalCQRed $PATT-$n.sca | cut -c 40- | tr -d '\n' >> "$tmp";
	    echo -n ", " >> "$tmp";
	done
	echo "\n" >> "$tmp";
	##########################
	# lifetime
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
