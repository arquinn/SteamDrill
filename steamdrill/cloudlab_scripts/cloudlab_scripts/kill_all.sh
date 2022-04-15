#!/bin/bash

host="$1"

for i in {0..7}; do
    echo node-$i.$host

#    echo 'pkill -9 streamserver' | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo 'pkill -9 pound_cpu' | ssh -o StrictHostKeyChecking=no node-$i.$host&
#    echo 'pkill -9 analyzer' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'pkill -9 read_meminfo' | ssh -o StrictHostKeyChecking=no node-$i.$host

#    echo 'rm /local/src/clean_omniplay/logging/server/*out*' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm /local/src/clean_omniplay/logging/server/*err*' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'pkill -9 800001_8457fc' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm -r /replay_cache/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm /replay_logdb/rec_385053/\*' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm -r /dev/shm/*' | ssh -o StrictHostKeyChecking=no node-$i.$host   


done
