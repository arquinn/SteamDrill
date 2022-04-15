#!/bin/bash

source ~/.omniplay_setup

rec=$1
ckpt_file=$2
time_step=$3

echo "$rec"

./mkpartition_dalek $rec 1 -ds | ./find_checkpoint_locations.py $time_step > $ckpt_file

ckpts=$(cat $ckpt_file)
echo $ckpts
for ckpt in $ckpts; do
    $OMNIPLAY_DIR/test/resume --pthread $MY_RESUME_PTHREAD $rec --ckpt_at=$ckpt
    echo "finished taking $ckpt"
done


#replay_ckpts=`ls $rec/ckpt.* | awk '{split($0,a,"/"); split(a[4],b,"."); print b[2]}'`

#echo $replay_ckpts

#for ckpt in $replay_ckpts; do
#    $OMNIPLAY_DIR/test/resume --pthread $MY_RESUME_PTHREAD $rec --from_ckpt=$ckpt
#    echo "finished with $ckpt"
#done
