#! /bin/bash

cd /tmp/PNL/ouichefs/mkfs

gcc -Wall -o mkfs.ouichefs mkfs-ouichefs.c

cd ..

#A changer pour en fonction du path
make KERNELDIR=/tmp/PNL/linux-6.5.7

#A changer pour en fonction du path
cp ouichefs.ko /users/nfs/Etu8/28634278/PNL/lkp-lap_02-src/src/scripts/share
