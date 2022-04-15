#!/bin/bash

test_dir=$1
output_dir=$2
global_test=$3

mkdir -p $output_dir

#for t in `ls $test_dir`; do
#output=$output_dir/$t
#test=$test_dir/$t
#echo $test $output
#./run.py -t $test --hosts emulab_hosts -s /local/src/clean_omniplay/logging/server -r 5 -o $output --skip-sync --ckpt --save-output --clear-ckpts --skip-check
#done

./prep_tree_test.sh $output_dir

mkdir -p $output_dir.tree
./run.py -t $global_test --hosts emulab_hosts -s /local/src/clean_omniplay/logging/server -r 5 -o $output_dir.tree --skip-sync --ckpt --save-output --clear-ckpts --skip-check --tree-merge=$output_dir.flat/0


#for t in `ls $test_dir`; do
#output=$output_dir/$t
#test=$test_dir/$t
#echo $test $output
#./run.py -t $test --hosts emulab_hosts -s /local/src/clean_omniplay/logging/server -r 5 -o $output --skip-sync --ckpt --save-output --clear-ckpts --skip-check
#done
