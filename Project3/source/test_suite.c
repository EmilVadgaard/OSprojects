#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

static char *read_lorem(const char *path, size_t size);

int main(int argc, char *argv[])
{ 
    if (argc != 2) return -1;

    char mnt[PATH_MAX];
    if (!realpath(argv[1], mnt)) return -1;

    char dir_path[PATH_MAX];
    char file_pathA[PATH_MAX];
    char file_pathB[PATH_MAX];
    char file_pathC[PATH_MAX];

    strcpy(dir_path,  mnt);      
    strcat(dir_path,  "/dirA");

    strcpy(file_pathA, dir_path); 
    strcat(file_pathA, "/fileA");

    strcpy(file_pathB, dir_path); 
    strcat(file_pathB, "/fileB");

    strcpy(file_pathC, dir_path); 
    strcat(file_pathC, "/fileC");

    mkdir(dir_path, 0755);

    creat(file_pathA, 0644);
    creat(file_pathB, 0644);
    creat(file_pathC, 0644);

    char *big_file = read_lorem("data/lorem.txt", 5000);
    char *small_file = read_lorem("data/lorem.txt", 1000);

    int fileA = open(file_pathA, O_WRONLY);
    if (fileA == -1) return -1;
    write(fileA, big_file, strlen(big_file));
    close(fileA);
    free(big_file);

    int fileB = open(file_pathB, O_WRONLY);
    if (fileB == -1) return -1;
    write(fileB, small_file, strlen(small_file));
    close(fileB);
    free(small_file);

    const char *msg = "Here lies some data!\n";
    int fileC = open(file_pathC, O_WRONLY);
    if (fileC == -1) return -1;
    write(fileC, msg, strlen(msg));
    close(fileC);


    printf("success\n");
    return 0;
}

static char *read_lorem(const char *path, size_t size)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *buf = malloc(size + 1);
    if (!buf) return -1;

    size_t amount_read = fread(buf, 1, size, f);

    buf[amount_read] = '\0';
    fclose(f);
    return buf;
}