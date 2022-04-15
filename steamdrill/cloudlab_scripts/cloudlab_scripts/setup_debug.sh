#!/bin/bash

host="$1"

for i in {0..15}; do 
    echo node-$i.$host

    echo 'echo /tmp/core.%p | sudo tee /proc/sys/kernel/core_pattern' | ssh -o StrictHostKeyChecking=no node-$i.$host
    echo 'echo 1 | sudo tee /proc/sys/kernel/replay_min_debug' | ssh -o StrictHostKeyChecking=no node-$i.$host   
done
