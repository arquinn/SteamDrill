#!/bin/bash

command=$1
counter=1
while [ -e $command.out.$counter ] 
do 
    counter=$((counter+1))
done

stdout=$command.out.$counter
stderr=$command.err.$counter


nohup $command 1>$stdout 2>$stderr&
