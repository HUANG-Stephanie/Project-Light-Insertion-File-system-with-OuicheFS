#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define SIZE 4096

int main(){

    int fd;
    if((fd = open("./ouichefs/test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }

    char data[14] = "World test !\0";
    char buf[SIZE];

    // Debut du bloc
    write(fd, data, 14);
    read(fd, buf, 14);
    printf("%s\n", buf);
    
    memset(buf, 0, SIZE);
    
    // Insertion début du bloc
    lseek(fd, 0, SEEK_SET);
    write(fd, "Hello", 5);
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 19);
    printf("%s\n", buf);

    // Insertion fin du bloc
    lseek(fd, strlen(buf), SEEK_SET);
    write(fd, "for project PNL", 15);
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 34);
    printf("%s\n", buf);

    // Insertion milieu du bloc
    lseek(fd, 5, SEEK_SET);
    write(fd, " Mr.", 4);
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 37);
    printf("%s\n", buf);

    memset(buf, 0, SIZE);

    // Insertion fin du bloc et creation nouveau bloc 
    lseek(fd, 4090, SEEK_SET);
    write(fd, "Je suis à l'offset 4090", 23);
    lseek(fd, 4090, SEEK_SET);
    read(fd, buf, 23);
    printf("%s\n", buf);

    char *data2 = malloc(sizeof(char)*SIZE);
    memset(data2, 'A', SIZE);
    memset(buf, 0, SIZE);

    // Insertion donnée de taille d'1 bloc au debut, décalage de tous les blocs
    lseek(fd, 0, SEEK_SET);
    write(fd, data2, 4096);
    lseek(fd, 4090, SEEK_SET);
    read(fd, buf, 30);
    printf("%s\n", buf);

    memset(buf, 0, SIZE);

    lseek(fd, 8192, SEEK_SET);
    read(fd, buf, SIZE);
    printf("%s\n", buf);

    free(data2);

    close(fd);
    return 0;
}