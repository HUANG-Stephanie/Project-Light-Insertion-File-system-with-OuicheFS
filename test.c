#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(){

    int fd;
    if((fd = open("./ouichefs/test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }

    char data[14] = "Hello test !\0";
    char buf[14];
    write(fd, data, 14);
    read(fd, buf, 14);
    printf("%s\n", buf);

    close(fd);
    return 0;
}