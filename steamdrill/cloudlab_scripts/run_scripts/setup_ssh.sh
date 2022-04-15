#!/bin/bash

host="$1"
num=$2

# get public key for the main node
#echo 'echo -e "y\n"| sudo ssh-keygen -f ~/.ssh/id_rsa -N "" ' | ssh node-$main.$host
##echo 'sudo chmod 740 ~/.ssh/id_rsa' | ssh node-$main.$host
#scp -o StrictHostKeyChecking=no node-$main.$host:~/.ssh/id_rsa.pub ~/.ssh/id_rsa_node-$main.$host.pub


for ((i = 0; i <=$num; i++)); do 
    echo node-$i.$host

    scp -o StrictHostKeyChecking=no ~/.ssh/id_rsa.cloudlab.pub node-$i.$host:~/.ssh/id_rsa.cloudlab.pub
    echo "cat ~/.ssh/id_rsa.cloudlab.pub >> ~/.ssh/authorized_keys" | ssh node-$i.$host


done

