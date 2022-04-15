#!/bin/bash
host="$1"
benchmark="$2"
dest="$3"

echo $host $dest/$benchmark

echo "mkdir -p $dest/$benchmark" | ssh $host


for i in `ls $benchmark`

do 
    if ! [[ $i = *"output"* ]]; 	
    then
	if ! [[ $i = *"timings"* ]];
	then
	    if ! [[ $i = *"result"* ]];
	    then echo $i;
	    scp -r -o StrictHostKeyChecking=no "$benchmark/$i" $host:"$dest/$benchmark/$i"
	    fi
	fi
    fi
done



