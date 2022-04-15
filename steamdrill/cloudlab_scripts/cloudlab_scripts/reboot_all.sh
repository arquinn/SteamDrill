#!/bin/bash

host="$1"

for i in {0..15}; do 
    echo node-$i.$host   
    echo 'sudo reboot' | ssh -o StrictHostKeyChecking=no node-$i.$host
done
