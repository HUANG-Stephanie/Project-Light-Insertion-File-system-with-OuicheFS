#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define CMD _IOWR('N', 0 , char* )

int main(){

    int fd, fd2, fd3;

    if((fd = open("/dev/ouichefs_ioctl", O_RDWR)) < 0){
        fprintf(stderr, "Error while opening /dev/ouichefs_ioctl\n");
        return -1;
    }

    if((fd2 = open("./ouichefs/ioctl_test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }
    
    if((fd3 = open("./ouichefs/test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }

    char data[14] = "Hello ioctl !\0";
    char buf[14];
    write(fd2, data, 14);
    read(fd2, buf, 14);
    printf("%s\n", buf);

    int res = ioctl(fd, CMD, fd2);
    if(res < 0){
        perror("ioctl failed\n");
        return -1;
    }

    char data2[14] = "Hello test !\0";
    char buf2[5000];

    write(fd3, data2, 14);
    read(fd3, buf2, 14);
    printf("%s\n", buf2);
    
    lseek(fd3, 0, SEEK_SET);
    write(fd3, "World", 5);
    lseek(fd3, 0, SEEK_SET);
    read(fd3, buf2, 19);
    printf("%s\n", buf2);

    lseek(fd3, 4090, SEEK_SET);
    write(fd3, "Je suis à l'offset 4090", 23);
    lseek(fd3, 4090, SEEK_SET);
    read(fd3, buf2, 23);
    printf("%s\n", buf2);

    char *data3 = malloc(sizeof(char)*4096);
    memset(data3, 'A', 4096);

    lseek(fd3, 0, SEEK_SET);
    write(fd3, data3, 4096);
    lseek(fd3, 4090, SEEK_SET);
    read(fd3, buf2, 30);
    printf("%s\n", buf2);
    
    res = ioctl(fd, CMD, fd3);
    if(res < 0){
        perror("ioctl failed\n");
        return -1;
    }

    close(fd);
    close(fd2);
    close(fd3);

    return 0;
}