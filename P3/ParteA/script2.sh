#! /bin/bash



for i in `seq 0 100`
do
	echo remove $i > /proc/modlist
	cat /proc/modlist
	sleep 1
done


