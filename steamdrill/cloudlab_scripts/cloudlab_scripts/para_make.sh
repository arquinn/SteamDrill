#!/bin/bash
host="$1"

for i in {0..15}; do 
    echo node-$i.$host

#    scp -o StrictHostKeyChecking=no  ../run_scripts/tree_merge.py node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
#    scp -o StrictHostKeyChecking=no  ../run_scripts/prep_tree_test.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
#    scp -o StrictHostKeyChecking=no  ../run_scripts/join_subtrees.py node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/

#    scp -o StrictHostKeyChecking=no  ../run_scripts/benchmark.py node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
    scp -o StrictHostKeyChecking=no  ../run_scripts/setup_checkpoint_dir.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
#    scp -o StrictHostKeyChecking=no  ../run_scripts/link_checkpoints.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
#    scp -o StrictHostKeyChecking=no  ../run_scripts/clear_checkpoints.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/

#    scp -o StrictHostKeyChecking=no  ../run_scripts/run.py node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
#    scp -o StrictHostKeyChecking=no  ../run_scripts/make_emulab_hosts.py node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/make_emulab_hosts.py
#    scp -o StrictHostKeyChecking=no  ../run_scripts/set_timing.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/
#    scp -o StrictHostKeyChecking=no  ../run_scripts/server.py node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/server.py
#    scp -o StrictHostKeyChecking=no  ../run_scripts/validate_output.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/validate_output.sh
#    scp -o StrictHostKeyChecking=no  ../run_scripts/remove_output.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/remove_output.sh
#    scp -o StrictHostKeyChecking=no  ../run_scripts/reset_host.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/reset_host.sh
#    scp -o StrictHostKeyChecking=no  ../run_scripts/clear_checkpoints.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/clear_checkpoints.sh
#     scp -o StrictHostKeyChecking=no ../run_scripts/run_background_task.sh node-$i.$host:/local/src/clean_omniplay/logging/orchestration/run_scripts/

#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/ptrace_tool/*.h node-$i.$host:/local/src/clean_omniplay/logging/ptrace_tool/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/ptrace_tool/*.cpp node-$i.$host:/local/src/clean_omniplay/logging/ptrace_tool/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/ptrace_tool/*.c node-$i.$host:/local/src/clean_omniplay/logging/ptrace_tool/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/ptrace_tool/Makefile node-$i.$host:/local/src/clean_omniplay/logging/ptrace_tool/Makefile
#    echo 'cd /local/src/clean_omniplay/logging/ptrace_tool/;make clean; make -j 5' | ssh node-$i.$host&

#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/server/read_meminfo.sh node-$i.$host:/local/src/clean_omniplay/logging/server/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/server/*.h node-$i.$host:/local/src/clean_omniplay/logging/server/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/server/config.cpp node-$i.$host:/local/src/clean_omniplay/logging/server/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/server/*.c node-$i.$host:/local/src/clean_omniplay/logging/server/
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/server/Makefile node-$i.$host:/local/src/clean_omniplay/logging/server/Makefile
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/server/streamctl.cpp node-$i.$host:/local/src/clean_omniplay/logging/server/
#    echo 'cd /local/src/clean_omniplay/logging/server/;source ~/.omniplay_setup; make -j 5' | ssh node-$i.$host&

#    echo '/bin/mkdir /local/src/clean_omniplay/logging/analyzer' | ssh node-$i.$host&
#$    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/analyzer/*.h node-$i.$host:/local/src/clean_mniplay/logging/analyzer
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/analyzer/*.cpp node-$i.$host:/local/src/clean_omniplay/logging/analyzer
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/analyzer/*.c node-$i.$host:/local/src/clean_omniplay/logging/analyzer
#    scp -o StrictHostKeyChecking=no  ~/Documents/omniplay/logging/analyzer/Makefile node-$i.$host:/local/src/clean_omniplay/logging/analyzer/
#    echo 'cd /local/src/clean_omniplay/logging/analyzer/;make clean; make -j 5' | ssh node-$i.$host&


done
