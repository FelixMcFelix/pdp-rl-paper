#!/bin/bash
SCRIPT="./quant-perf-tput-task.py"

for concurrent_task_limit in `seq 1 32`;
do
	for quantiser_choice in `seq 0 3`;
	do
		for do_update in `seq 0 1`;
		do
			for attempt in `seq 0 0`;
			do
				for task_i in $(seq 0 $(expr $(concurrent_task_limit) - 1));
				do
					python2 $SCRIPT $concurrent_task_limit $task_i $quantiser_choice $do_update $attempt &
				done

				wait
			done
		done
	done
done

