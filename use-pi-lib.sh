#!/usr/bin/env bash

if grep --quiet ARM /proc/cpuinfo
then
	echo -n -lwiringPi -lasound
else
	echo -n -lpulse -lpulse-simple
fi
