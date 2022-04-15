#!/bin/bash

host="$1"

for i in {0..15}; do 
    echo node-$i.$host
    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/eglibc-2.15/prefix.tgz node-$i.$host:/local/src/clean_omniplay/eglibc-2.15/
    echo 'cd /local/src/clean_omniplay/eglibc-2.15; tar -xvf prefix.tgz' | ssh node-$i.$host&                                           #make the test directory



done
