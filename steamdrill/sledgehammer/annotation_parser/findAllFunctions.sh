#!/bin/bash

filepath=$1
regex=$2



mangled=`nm $filepath | awk '{print $3}'`

for i in $mangled
do 
#    echo $i
    demanglei=`c++filt $i`
    grepSays=`echo $demanglei | egrep $regex`

    if [[ !  -z  $grepSays  ]]
    then 
#	echo $demanglei
	echo $i

    fi 
#    grepSays=`echo $demanglei | egrep $2`
#    echo $grepSays

done
