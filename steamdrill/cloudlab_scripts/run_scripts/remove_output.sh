#!/bin/bash

dir=$1

echo "removing from $dir" 


rm $dir/*/output
rm $dir/*/altrace.*
