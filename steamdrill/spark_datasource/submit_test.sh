#!/usr/bin/env bash

test=$1
spark_dir=$2


### Start the steamdrill server
#pushd ../server;
#./steamdrill_server&
#server_pid=$!
#echo "[$server_pid] Running Steamdrill Server"
#popd


tmp_data=/tmp/Results
echo "Running $test with dir $spark_dir"



install_jar="build/libs/spark_datasource-1.0-all.jar"
native=`pwd .`/build/libs/shm_iter/shared

export STEAMDRILL_NUM_CORES=1
export STEAMDRILL_INSTALL_DIR=`dirname $0`
export STEAMDRILL_GEN_STATS=/tmp/Results-generator
export STEAMDRILL_JUMP_COUNTER="all"
export STEAMDRILL_LOG_LEVEL="i"
export STEAMDRILL_TAG="0"
export STEAMDRILL_ASSEMBLY_REORDER="True"
#export STEAMDRILL_ASSEMBLY_REORDER="False"

#~/src/spark-for-OmniTable/bin/spark-submit\
mkdir -p "/tmp/$STEAMDRILL_TAG"

$spark_dir/bin/spark-submit\
    --properties-file $spark_dir/bin/configuration\
    --conf spark.eventLog.enabled=true\
    --conf spark.eventLog.dir="/tmp/$STEAMDRILL_TAG"\
		--conf spark.driver.extraLibraryPath=$native\
		--conf spark.executor.extraLibraryPath=$native\
		--conf "spark.executorEnv.STEAMDRILL_ASSEMBLY_REORDER=$STEAMDRILL_ASSEMBLY_REORDER"\
		--conf "spark.executorEnv.STEAMDRILL_NUM_CORES=$STEAMDRILL_NUM_CORES"\
		--conf "spark.executorEnv.STEAMDRILL_INSTALL_DIR=$STEAMDRILL_INSTALL_DIR"\
		--conf "spark.executorEnv.STEAMDRILL_GEN_STATS=$STEAMDRILL_GEN_STATS"\
		--conf "spark.executorEnv.STEAMDRILL_JUMP_COUNTER=$STEAMDRILL_JUMP_COUNTER"\
		--conf "spark.executorEnv.STEAMDRILL_LOG_LEVEL=$STEAMDRILL_LOG_LEVEL"\
		--conf "spark.executorEnv.STEAMDRILL_TAG=$STEAMDRILL_TAG"\
    --master local[1]\
    --class examples.$test\
    $install_jar


### Stop the steamdrill server
#echo "Killing Steamdrill Server"
#kill $server_pid
