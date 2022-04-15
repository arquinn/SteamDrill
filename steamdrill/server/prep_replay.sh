#!/bin/bash


if [ $# -lt 1 ]; then
    echo "usage:"
    echo "./prep_replay.sh <replay_name> [num_hosts]"
    exit 1
fi

base=$1
replay="/replay_logdb/$base"
if [ $# -ge 2 ]; then
    numHosts=$2
else
    numHosts=8
fi


## reduce the strain on nfs by moving replay_logdb directories over to
## /replay_logdb directory

for i in $(seq 0 $((numHosts-1))); do 
    echo "if [[ -L '/replay_logdb' ]]; then sudo rm /replay_logdb; sudo mkdir /replay_logdb; sudo chmod 777 /replay_logdb; fi" | ssh -o StrictHostKeyChecking=no -q node-$i
    echo "if [[ ! -d $replay ]]; then cp -r /proj/dift-PG0/replay_logdb/$base/ $replay; fi" | ssh -o StrictHostKeyChecking=no -q node-$i&
done
wait

##add all the blocks in case they aren't there. 
for i in $(seq 0 $((numHosts-1))); do 
    echo "pushd $OMNIPLAY_DIR/steamdrill/server; ./block_cache_add.sh $replay" | ssh -o StrictHostKeyChecking=no -q node-$i&
done
wait

pushd $OMNIPLAY_DIR/steamdrill/server > /dev/null
#./block_cache_add.sh $replay
ckpts=""
for i in $(seq 0 6); do
    ickpts=`./check_partition $replay $((2**i)) | awk '$5 >0 {print $5}'`
    ckpts="$ckpts $ickpts"
done
popd > /dev/null
unique_ckpt_vals=`echo $ckpts | tr " " "\n" | sort -nu`


#with the power of distributed file systems, this should sync everything!
sudo mkdir -p /mnt/ckpts$replay
sudo chown arquinn:dift-PG0 /mnt/ckpts$replay
sudo chmod 777 /mnt/ckpts$replay

for i in $(seq 0 $((numHosts-1))); do 
    echo "sudo ln -s /mnt/ckpts$replay $replay/1" | ssh -o StrictHostKeyChecking=no -q node-$i&
done
wait

## now take all of the checkpoints that we need to take!

## there is a clever way of doing this where it tries until they're all filled!!

nxtNode=0
ran=1

while  [ $ran -gt 0 ]; do
    ran=0
    for i in $unique_ckpt_vals; do 
	if [ ! -e "$replay/1/ckpt.$i" ]; then
	    echo "$OMNIPLAY_DIR/test/resume --pthread $OMNIPLAY_DIR/eglibc-2.15/prefix/lib --ckpt_at=$i $replay" | ssh -o StrictHostKeyChecking=no -q node-$nxtNode&
	    nxtNode=$(($(($nxtNode + 1)) % $numHosts))
	    ran=1
	fi
    done
    wait
done


