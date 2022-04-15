#!/bin/bash

host="$1"

for i in {0..15}; do 
    echo node-$i.$host
#    echo 'rm -r /dev/shm/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo 'rm /local/src/clean_omniplay/logging/server/*out*' | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo 'rm /local/src/clean_omniplay/logging/server/*err*' | ssh -o StrictHostKeyChecking=no node-$i.$host

#    echo 'rm -r /replay_cache/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo 'rm -r /replay_logdb/rec_385035/ckpt.*' | ssh -o StrictHostKeyChecking=no node-$i.$host

done
