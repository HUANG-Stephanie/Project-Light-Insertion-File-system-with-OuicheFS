#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define SIZE_MIN 1024
#define SIZE_MAX 4096
#define NB_FIC 5
#define AJOUT 10

void create_file(char* filename, size_t size){

    double time_read = 0.0;
    double time_write = 0.0;
    clock_t begin;
    clock_t end;
    int offset;
    char data[size];
    char chaine[size];
    chaine[size-1] = '\0';

    FILE * file = NULL;
    if((file = fopen(filename, "w+")) == NULL){
        printf("Error: not open\n");
        return;
    }
    
    for(int i=0; i<AJOUT; i++){
        sprintf(data, "%c", 'A'+(i%26));
        offset = rand()%size;
        
        //Write
        fseek(file, offset, SEEK_SET);
        begin = clock();
        fputs(data, file);
        end = clock();
        time_write += (double)(end - begin) / CLOCKS_PER_SEC;

        //Read
        fseek(file, offset, SEEK_SET);
        begin = clock();
        fgets(chaine, size, file);
        end = clock();
        time_read += (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Lecture : %s\n", chaine);

    }

    printf("The write times is %f seconds\n", time_write);
    printf("The read times is %f seconds\n", time_read);

    fclose(file);
}

int main(){

    srand(time(NULL));
    int size;
    char filename[255];
    char* dirName = "./ouichefs/fichiers";

    mkdir(dirName, 0755);

    for(int i=0; i<NB_FIC; i++){
        size = SIZE_MIN + (rand() % SIZE_MAX);
        sprintf(filename, "./%s/%c.txt", dirName, 'A'+(i%26));
        create_file(filename, size);
    }

    return 0;
}