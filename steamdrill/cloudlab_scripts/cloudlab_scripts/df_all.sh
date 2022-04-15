#!/bin/bash

host="$1"

for i in {0..15}; do 
    echo node-$i.$host

    echo 'df' | ssh -o StrictHostKeyChecking=no node-$i.$host

#    ping -qc 4 node-$i.$host
#    echo 'rm -r /tmp/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
#    echo 'rm -r /dev/shm/*' | ssh -o StrictHostKeyChecking=no node-$i.$host
done
