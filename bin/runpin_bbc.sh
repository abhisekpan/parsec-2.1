#!/bin/bash                                                                                                                         
TOOLNAME=bbcollection
TOOLDIR=PinBBCollection
#find the number of cores that parsec is going to use and use that to set the 
#number of cores in the pintool

[ -z "$PARSEC_CPU_NUM" ] && echo "Please set PARSEC_CPU_NUM" && exit 1
numcores=$PARSEC_CPU_NUM
[ -z "$PIN_OUTPUT" ] && echo "Please set PIN_OUPUT with the output file name" && exit 1
outfile=$PIN_OUTPUT
[ -z "$PIN_VERSION" ] && echo "Please set PIN_VERSION with the version of PIN to use" && exit 1
PINDIR=~/$PIN_VERSION-gcc.3.4.6-ia32_intel64-linux
PINTOOLDIR=$PINDIR/source/tools/$TOOLDIR

$PINDIR/pin -pause_tool 10 -separate_memory -t $PINTOOLDIR/obj-intel64/$TOOLNAME.so -c $numcores -o $outfile -- "$@"

