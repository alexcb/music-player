#!/usr/bin/env bash

if grep --quiet ARM /proc/cpuinfo
then
	echo -n -DUSE_RASP_PI=1
fi
