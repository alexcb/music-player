#!/usr/bin/env bash

if grep --quiet ARM /proc/cpuinfo
then
	echo -n -DUSE_RASP_PI=1 -O3
else
	echo -n -g
fi

if [[ $(hostname -f) = "music-kitchen" ]]; then
	echo -n " -DKITCHEN"
fi


