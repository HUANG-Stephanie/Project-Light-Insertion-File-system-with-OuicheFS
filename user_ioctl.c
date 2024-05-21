#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define CMD _IOWR('N', 0 , char* )

int main(){

    int fd, fd2;
    if((fd = open("/dev/ouichefs_ioctl", O_RDWR)) < 0){
        fprintf(stderr, "Error while opening /dev/ouichefs_ioctl\n");
        return -1;
    }

    if((fd2 = open("./ouichefs/ioctl_test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }

    char data[14] = "Hello ioctl !\0";
    char buf[14];
    write(fd2, data, 14);
    read(fd2, buf, 14);
    //printf("%s\n", buf);

    int res = ioctl(fd, CMD, fd2);
    if(res < 0){
        perror("ioctl failed\n");
        return -1;
    }
    
    close(fd);
    close(fd2);
    return 0;
}