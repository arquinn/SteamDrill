#!/usr/bin/env bash
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

host=`cat /proc/sys/kernel/hostname`
#host=singleton5.eecs.umich.edu
install_jar="build/libs/spark_datasource-1.0-all.jar"
native=`pwd .`/build/libs/shm_iter/shared

SCRIPT=$(readlink -f $0)

export STEAMDRILL_NUM_CORES=64
export STEAMDRILL_INSTALL_DIR=`dirname $SCRIPT`
export STEAMDRILL_JUMP_COUNTER="all"
export STEAMDRILL_LOG_LEVEL="i"
export STEAMDRILL_TAG="0"
export STEAMDRILL_ASSEMBLY_REORDER="True"

#~/src/spark-for-OmniTable/bin/spark-submit\

mkdir -p /tmp/$STEAMDRILL_TAG

$spark_dir/bin/spark-submit\
    --properties-file $spark_dir/bin/configuration\
    --conf spark.eventLog.enabled=true\
    --conf spark.eventLog.dir="/tmp/$STEAMDRILL_TAG"\
		--conf spark.driver.extraLibraryPath=$native\
		--conf spark.executor.extraLibraryPath=$native\
    --conf spark.locality.wait.node=3600s\
		--conf "spark.executorEnv.STEAMDRILL_ASSEMBLY_REORDER=$STEAMDRILL_ASSEMBLY_REORDER"\
		--conf "spark.executorEnv.STEAMDRILL_NUM_CORES=$STEAMDRILL_NUM_CORES"\
		--conf "spark.executorEnv.STEAMDRILL_INSTALL_DIR=$STEAMDRILL_INSTALL_DIR"\
		--conf "spark.executorEnv.STEAMDRILL_JUMP_COUNTER=$STEAMDRILL_JUMP_COUNTER"\
		--conf "spark.executorEnv.STEAMDRILL_LOG_LEVEL=$STEAMDRILL_LOG_LEVEL"\
		--conf "spark.executorEnv.STEAMDRILL_TAG=$STEAMDRILL_TAG"\
		--master spark://$host:7077\
    --class examples.$test\
		--total-executor-cores=$STEAMDRILL_NUM_CORES\
    --executor-cores=2\
    $install_jar

### Stop the steamdrill server
#echo "Killing Steamdrill Server"
#kill $server_pid
