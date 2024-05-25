mkdir /tmp/ouichefs

dd if=/dev/zero of=ouichefs.img bs=1M count=50

insmod ../share/ouichefs.ko

mkfs.ouichefs ouichefs.img

mount -o loop ouichefs.img /tmp/ouichefs

mknod /dev/ouichefs_ioctl c 248 0

chmod 666 /dev/ouichefs_ioctl

cd /tmp
