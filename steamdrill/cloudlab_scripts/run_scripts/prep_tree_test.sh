#!/bin/bash


output=$1

mkdir $output.imm
for i in `ls $output`; do
    cp -r $output/$i/* $output.imm
done

mkdir $output.flat
for i in `ls $output.imm`; do
    ./join_subtrees.py $output.imm/$i $output.flat/$i
    
done

rm -r $output.imm
