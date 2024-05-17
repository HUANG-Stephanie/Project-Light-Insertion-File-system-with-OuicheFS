#! /bin/bash

mkdir /tmp/PNL
cp /usr/data/sopena/pnl_2023-2024/lkp-arch.img /tmp/PNL
cp /usr/data/sopena/pnl_2023-2024/linux-6.5.7.tar.xz /tmp/PNL

cd /tmp/PNL

tar xf linux-6.5.7.tar.xz

cd linux-6.5.7

make defconfig

make -j 24
