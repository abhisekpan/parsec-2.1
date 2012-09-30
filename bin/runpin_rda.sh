#!/bin/bash                                                                                                                         
TOOLNAME=reusedistance
TOOLDIR=PinReuseDistance
PINDIR=~/pin-2.8-37300-gcc.3.4.6-ia32_intel64-linux
PINTOOLDIR=$PINDIR/source/tools/$TOOLDIR
#find the number of cores that parsec is going to use and use that to set the 
#number of cores in the pintool

[ -z "$PARSEC_CPU_NUM" ] && echo "Please set PARSEC_CPU_NUM" && exit 1
numcores=$PARSEC_CPU_NUM
[ -z "$PIN_OUTPUT" ] && echo "Please set PIN_OUPUT with the output file name" && exit 1
outfile=$PIN_OUTPUT

[ -z "$PIN_INTERVAL_SIZE" ] && echo "Please set PIN_INTERVAL_SIZE with the number of instructions in each interval" && exit 1
interval_size=$PIN_INTERVAL_SIZE

#$PINDIR/pin -pause_tool 50 -t $PINTOOLDIR/obj-intel64/$TOOLNAME.so -i 5000000 -c $numcores -o $outfile -- "$@"

#share-aware private
#$PINDIR/pin -pause_tool 80 -separate_memory -t $PINTOOLDIR/obj-intel64/$TOOLNAME.so -n false -s private -i $interval_size -c $numcores -o $outfile -- "$@"

#shared
$PINDIR/pin -pause_tool 80 -separate_memory -t $PINTOOLDIR/obj-intel64/$TOOLNAME.so -r false -s shared -i $interval_size -c $numcores -o $outfile -- "$@"
