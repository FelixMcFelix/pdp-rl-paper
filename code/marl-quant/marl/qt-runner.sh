#!/bin/bash
SCRIPT="./quant-train.py"

getCount() {
	python $SCRIPT $1
}
runExpt() {
	python $SCRIPT $1 $2
}

./purger.sh

for model in `seq 0 0`;
do
	for i in $(seq 0 $(expr $(getCount $model) - 1));
	do
		./purger.sh
		runExpt $model $i
	done
done

./purger.sh

