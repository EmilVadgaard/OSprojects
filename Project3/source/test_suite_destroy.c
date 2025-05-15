#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_NAME_LEN 4096

int main(int argc, char *argv[])
{ 
    if (argc != 2) return -1;

    char mnt[MAX_NAME_LEN];
    if (!realpath(argv[1], mnt)) return -1;

    char dir_path[MAX_NAME_LEN];
    char file_pathA[MAX_NAME_LEN];
    char file_pathB[MAX_NAME_LEN];
    char file_pathC[MAX_NAME_LEN];

    strcpy(dir_path,  mnt);      
    strcat(dir_path,  "/dirA");

    strcpy(file_pathA, dir_path); 
    strcat(file_pathA, "/fileA");

    strcpy(file_pathB, dir_path); 
    strcat(file_pathB, "/fileB");

    strcpy(file_pathC, dir_path); 
    strcat(file_pathC, "/fileC");

    unlink(file_pathA);
    unlink(file_pathB);
    unlink(file_pathC);

    rmdir(dir_path);

    return 0;
}