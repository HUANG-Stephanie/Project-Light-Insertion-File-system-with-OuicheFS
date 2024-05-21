#! /bin/bash

#installe ouichefs et fait un premier make et met le module dans le dossier
#share

#Path a changer en fonction des prefs
cd /tmp/PNL/

git clone https://github.com/rgouicem/ouichefs.git

cd /tmp/PNL/ouichefs/mkfs

gcc -Wall -o mkfs.ouichefs mkfs-ouichefs.c

cd ..

#Path a changer en fonction des prefs
make KERNELDIR=/tmp/PNL/linux-6.5.7

#Path a changer en fonction des prefs
cp ouichefs.ko /users/nfs/Etu8/28634278/PNL/lkp-lap_02-src/src/scripts/share
