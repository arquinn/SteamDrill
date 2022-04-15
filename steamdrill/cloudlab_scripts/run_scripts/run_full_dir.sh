#!/bin/bash

#ironically... these are the same
test_dir=$1
output_dir=$2
global_test=$3


mkdir -p $output_dir

for t in `ls $test_dir`; do 
    if [ -d $test_dir/$t ]
    then echo $test_dir/$t;
	subtest=$test_dir/$t
	suboutput=$output_dir/$t
	
	./run_all.sh $subtest $suboutput $global_test

    fi

done

