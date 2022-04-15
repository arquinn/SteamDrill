#!/bin/bash

host="$1"
ckpt_dir="$2"
replay_dir="$3"

glibc="/home/arquinn/Documents/omniplay/eglibc-2.15/prefix/lib/"
glibc_cl="/local/src/clean_omniplay/eglibc-2.15/prefix/lib/"

for i in {0..15}; do 
    echo node-$i.$host

    echo "sudo mkdir /home/arquinn; sudo chown arquinn /home/arquinn; sudo /bin/mkdir -p $glibc; sudo chmod -R 777 /home/arquinn"  | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo "ln -s $glibc_cl/ld-2.15.so $glibc/ld-2.15.so "  | ssh -o StrictHostKeyChecking=no node-$i.$host

    echo "sudo mkdir $replay_dir" | ssh -o StrictHostKeyChecking=no node-$i.$host    
    echo "sudo chmod 777 $replay_dir" | ssh -o StrictHostKeyChecking=no node-$i.$host    
    echo "rm $replay_dir/ckpt.* " | ssh -o StrictHostKeyChecking=no node-$i.$host
    
    dir="ln -s /proj/dift-PG0/$ckpt_dir/"
    rp_dir="$replay_dir"
    echo $dir/ckpt.* $rp_dir  | ssh -o StrictHostKeyChecking=no node-$i.$host
done;

