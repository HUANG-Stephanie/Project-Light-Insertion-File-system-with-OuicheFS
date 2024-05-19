#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define CMD _IOWR('N', 0 , char* )

int main(){

    int fd, fd_fic;
    if(fd = open("/dev/ouichefs_ioctl", O_RDWR) < 0){
        fprintf(stderr, "Error while opening /dev/ouichefs_ioctl\n");
        return -1;
    }

    if((fd_fic = open("./ioctl_test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }

    char data[13] = "Hello ioctl !";
    char buf[13];
    //write(fd, data, 13);
    //read(fd, buf, 13);
    //printf("%s\n", buf);

    int res = ioctl(fd, CMD, fd_fic);
    if(res < 0){
        perror("ioctl failed\n");
        return -1;
    }

    close(fd);
    close(fd_fic);
    return 0;
}