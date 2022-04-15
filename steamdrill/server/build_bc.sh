#!/bin/bash

show_help() {
cat <<EOF
Usage : ${0##*/} [-h] [-p PROGRAM] [-f TRACERS] [-o OUTPUT] [-t TPS] [-k keep] [-i IPS] <list of program bcs to include>
Build a selected, isolated shared object based on output
      -h 	  Display this help and exit
      -f tracers  The tracer functions that will be executed
      -a assembly The file containing assembly snippets
      -t tps	  The tracerpoints file
      -i ips	  The place to put the instrumentaiton points file
      -o output	  The location to place the shared object
      -r replay	  The replay directory
      -k          Keep the temp files
EOF
}


# you're going to have to reset this if llvm is in a different location.
export LLVM_DIR=~/src/llvm/llvm-project/build/Release+Asserts
export PATH=$PATH:$LLVM_DIR/bin

#posix variable
OPTIND=1;  #Reset b/c stackoverflow says I should

functions=""
assembly=""
output=""
tracepoints=""
instpoints=""
replay=""
keep_around=0

while getopts "h?o:f:t:ki:r:a:" opt; do
    case "$opt" in
	h|\?)
	    show_help
	    exit 0
	    ;;
	o)
	    output=$OPTARG
	    ;;
	f)
	    functions=$OPTARG
	    ;;
	t)
	    tracepoints=$OPTARG
	    ;;
	i)
	    instpoints=$OPTARG
	    ;;
	r)
	    replay=$OPTARG
	    ;;
	a)
	    assembly=$OPTARG
	    ;;
	k)
	    keep_around=1
    esac
done

#globals (these don't depend on what the user wants to do)
LIB=`dirname $BASH_SOURCE`
EGALITO=$LIB/../lib/egalito/app/steamdrillInst
TRACER_LIBS=$LIB/../sledgehammer/tracer_library/
MUSL=$LIB/../lib/OmniTable-Musl/install
CONFIG_INC=$LIB/..
UTIL_LIBS=$LIB/../utils/

PASS_DIR=$LIB/../sledgehammer/isolation_pass/build/
SELECT_FUNCS=selectFunctions/libSelectFunctionsPass.so
ISOLATE=isolate/libIsolationPass.so


uuid=$(uuidgen)
TEMP="temp$uuid"
echo $TEMP
INC_FLAGS="-I$TRACER_LIBS -I$MUSL/include/ -I$CONFIG_INC -I$UTIL_LIBS"
LIB_UTIL="$LIB/../../test/libutil.a $MUSL/lib/libc.a"

#run through egalito:
as $assembly -c -o $assembly.o

#echo "clang -nostdinc $INC_FLAGS -c -emit-llvm -o $TEMP.bc $functions"
clang -nostdinc $INC_FLAGS -c -emit-llvm -o $TEMP.bc $functions
llvm-link -o $TEMP.link.bc $TEMP.bc $TRACER_LIBS/tracer_library.bc $TRACER_LIBS/shared_state.bc

opt -load $PASS_DIR/$ISOLATE -isolate -o=$TEMP.pass.bc -tps=$tracepoints $TEMP.link.bc

#perhaps there's a balance for -O3 at the end that can balance latency?
clang -c -o $TEMP.pass.o $TEMP.pass.bc
#clang -nostdlib -shared -o $output $TEMP.pass.bc $LIB_UTIL $assembly.o -O3
ld -nostdlib -shared -o $output $TEMP.pass.o $LIB_UTIL $assembly.o

#Cleanup, cleanup, everybody do your share!!!
if [ $keep_around -eq 0 ]
then
    rm $TEMP.bc $TEMP.link.bc $TEMP.pass.bc $TEMP.pass.o
fi
