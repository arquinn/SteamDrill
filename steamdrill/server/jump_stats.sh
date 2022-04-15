#!/bin/bash
SUM_ALL=0
SUM_PART=0
SUM_OPT=0

for file in blocks/*;  do
    all=`../instrument/parse_blocks $i | grep "jm=true"`
    let SUM_ALL=SUM_ALL+all

    part=`../instrument/parse_blocks $i | grep "unjm=true"`
    let SUM_PART=SUM_PART+part

    opt=`../instrument/parse_blocks $i | grep "unfjm=true"`
    let SUM_OPT=SUM_OPT+opt
    
done

echo "all=$SUM_ALL, part=$SUM_PART, opt=$SUM_OPT"

