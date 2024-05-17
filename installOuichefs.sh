#! /bin/bash

cd /tmp/PNL/

git clone https://github.com/rgouicem/ouichefs.git

gcc -Wall -o mkfs.ouichefs mkfs-ouichefs.c

cd ..
make KERNELDIR=/tmp/PNL/linux-6.5.7

#
cp ouichefs.ko /users/nfs/Etu8/28634278/PNL/lkp-lap_02-src/src/scripts/share
