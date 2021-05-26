#!/bin/sh

TARGET_FILE=/var/rl-perf-tester/progress
IGB_LOC=/home/clouduser
RL_PT_LOC=/home/clouduser/kyles/pdp-rl-paper/code/rl-perf-tester

if test -f "$TARGET_FILE"; then
	START_POINT=`cat $TARGET_FILE`
	rm $TARGET_FILE
	cd $IGB_LOC
	./igb.sh
	cd $RL_PT_LOC
	./target/release/rl-perf-tester stress-client --hack-progress $START_POINT cube3.maas
fi
