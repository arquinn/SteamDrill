#!/bin/bash

mkdir -p $2.ckpt
for i in {0..6};
do size=$((2**$i));
    echo "./mkpartition_dalek $1 $size -s -c > $2.ckpt/$size"
    ./mkpartition_dalek $1 $size -s -c > $2.ckpt/$size
done;
