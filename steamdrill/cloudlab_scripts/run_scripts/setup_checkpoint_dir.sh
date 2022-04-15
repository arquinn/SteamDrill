#!/bin/bash

rp_dir=$1

mkdir /dev/shm/ckpts
mkdir $rp_dir

ln -s /dev/shm/ckpts $rp_dir/7
echo 7 | sudo tee /proc/sys/kernel/replay_ckpt_dir
