#!/bin/sh
#
# Shell script to simulate fdformat by using superformat.
# (C) 1996 Michael Meskes <meskes@informatik.rwth-aachen.de>
# Placed under GPL.
#

set -e

#
# shall we verify?
#

if [ ! $1 ]
then
	echo "Usage: $0 [ -n ] device"
	exit 1
elif [ $1 = "-n" ]
then
	shift 1
	args="-f "
fi

#
# calculate parameters
#

format=`getfdprm $1`

capacity=`echo $format|cut -d' ' -f1`
capacity=`expr $capacity / 2`
sectors=`echo $format|cut -d' ' -f2`
heads=`echo $format|cut -d' ' -f3`
tracks=`echo $format|cut -d' ' -f4`
stretch=`echo $format|cut -d' ' -f5`
gap=`echo $format|cut -d' ' -f6`
rate=`echo $format|cut -d' ' -f7`
spec1=`echo $format|cut -d' ' -f8` # not needed
fmt_gap=`echo $format|cut -d' ' -f9`

device=`echo $1|sed -e 's/\/dev\/fd//' -e 's/\(u\|h\|H\|d\|D\|E\)[0-9]\+$//'`

args="$args -V -d /dev/fd$device -s $sectors -t $tracks -H $heads --stretch $stretch \
-g $gap -r $rate -G $fmt_gap"

headtext="Sing"
if [ $heads -eq 2 ]
then
	headtext="Doub"
fi

echo ${headtext}le-sided, ${tracks} tracks, ${sectors} sec/track. Total capacity ${capacity} kB.

superformat $args

