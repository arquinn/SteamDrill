#!/bin/bash

sudo echo 0 > /proc/sys/kernel/record_timings_flag
sudo echo 100 > /proc/sys/kernel/timing_buffer_size
