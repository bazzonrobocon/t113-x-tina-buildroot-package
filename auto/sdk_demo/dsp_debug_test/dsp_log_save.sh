#!/bin/sh

while true
do
	dsp_debug -d /dev/dsp_debug -r >> dsp_log.txt
	usleep 200000
done
