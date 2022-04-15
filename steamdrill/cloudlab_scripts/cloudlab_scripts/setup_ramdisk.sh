#!/bin/bash

host="$1"
rp_dir="$2"

for i in {0..15}; do 
    echo node-$i.$host

    echo "mkdir /dev/shm/ckpts" | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo "mkdir $rp_dir" | ssh -o StrictHostKeyChecking=no node-$i.$host

    echo "ln -s /dev/shm/ckpts $rp_dir/7" | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo "echo 7 | sudo tee /proc/sys/kernel/replay_ckpt_dir" | ssh -o StrictHostKeyChecking=no node-$i.$host


## actually, don't want to do this!! ### //and why not?




#    echo 'cat /local/src/omniplay/dift/proc64/streamserver.err' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'echo 1 | sudo tee /proc/sys/kernel/replay_min_debug' | ssh -o StrictHostKeyChecking=no node-$i.$host   
#    echo 'ps -Tu arquinn -o pid,uname,comm,pcpu' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm -r /tmp/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm -r /dev/shm/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
done
