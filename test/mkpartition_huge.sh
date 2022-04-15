#!/bin/bash

replay=$1
dest=$2

mkdir -p $dest
sizes="128 256 512 1024"

for i in $sizes;
do 
    echo "./mkpartition_dalek $replay $i -c -s > $dest/$i"
    ./mkpartition_dalek $replay $i -c -s > $dest/$i
done;
