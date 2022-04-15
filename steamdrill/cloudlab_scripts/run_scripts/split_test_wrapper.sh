#!/bin/bash

test_dir=$1
config_dir=$2
name=$3

echo "making huge for $name"
echo "splitting tests in $test_dir"
echo "putting configs in $config_dir"


base=$config_dir/base
global=$config_dir/global
dest=$config_dir/$name.huge

mkdir -p $dest

for i in `ls $test_dir`; do
    echo $i
    ./split_test.py $test_dir/$i $test_dir.$i.split/ 64
    ./make_configs.py $base $test_dir.$i.split $dest/$i
done


cp $global $dest/$name.global
