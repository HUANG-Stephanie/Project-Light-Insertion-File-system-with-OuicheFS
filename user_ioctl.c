#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(){

    int fd;
    if(fd = open("/dev/ouichefs_ioctl", O_RDONLY) < 0){
        fprintf(stderr, "Error while opening /dev/ouichefs_ioctl\n");
        return -1;
    }

    ioctl(fd);
}