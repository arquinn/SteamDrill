#!/usr/bin/env bash

test=$1

### Start the steamdrill server
#pushd ../server;
#./steamdrill_server&
#server_pid=$!
#echo "[$server_pid] Running Steamdrill Server"
#popd


tmp_data=/tmp/Results
echo "Running $test"

install_dir=`dirname $0`
# 
#~/src/spark-for-OmniTable/bin/spark-submit\
#--properties-file ~/src/spark-for-OmniTable/bin/configuration\

/local/src/spark-for-OmniTable/bin/spark-submit\
     --properties-file /local/src/spark-for-OmniTable/bin/configuration\
     --master local[1]\
     --class examples.$test\
     target/data-source-1.0-SNAPSHOT.jar\
     $tmp_data $install_dir 1




### Stop the steamdrill server
#echo "Killing Steamdrill Server"
#kill $server_pid
