#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define SIZE_MIN 1024
#define SIZE_MAX 4096
#define NB_FIC 5
#define AJOUT 10

void create_fd(char* fdname, size_t size){

    double time_read = 0.0;
    double time_write = 0.0;
    clock_t begin;
    clock_t end;
    int offset;
    char data[2];
    char chaine[2];
    int fd;
    
    if((fd = open(fdname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) == -1){
        perror("Error: not open\n");
        exit(EXIT_FAILURE);
    }

    for(int i=0; i<AJOUT; i++){
        sprintf(data, "%c", 'A'+(i%26));
        offset = rand()%size;
        
        //Write
        lseek(fd, offset, SEEK_SET);
        begin = clock();
        write(fd, data, 2);
        end = clock();
        time_write += (double)(end - begin) / CLOCKS_PER_SEC;

        //Read
        lseek(fd, offset, SEEK_SET);
        begin = clock();
        read(fd, chaine, 2);
        end = clock();
        time_read += (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Lecture : %s\n", chaine);

    }

    printf("The write times is %f seconds\n", time_write);
    printf("The read times is %f seconds\n", time_read);

    close(fd);
}

int main(){

    srand(time(NULL));
    int size;
    char fdname[255];
    char* dirName = "./ouichefs/fichiers";

    mkdir(dirName, 0755);

    for(int i=0; i<NB_FIC; i++){
        size = SIZE_MIN + (rand() % SIZE_MAX);
        sprintf(fdname, "./%s/%c.txt", dirName, 'A'+(i%26));
        create_fd(fdname, size);
    }

    return 0;
}
