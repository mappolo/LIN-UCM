#! /bin/bash



for i in `seq 0 100`
do
	echo add $i > /proc/modlist
	sleep 1
done


