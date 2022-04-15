#!/bin/bash

main_dir=$1
correct=$2

dirs=`ls $main_dir`

for i in $dirs; do 
    diff $correct $main_dir/$i/output
done
