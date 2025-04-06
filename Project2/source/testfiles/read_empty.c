#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[]){
    pid_t pid;
    int fd;

    char msg[] = "Hello";
    char out[20];
    int check = 0;

    pid = fork();
    
    if (pid == 0){
        fd = open("/dev/dm510-0", O_RDWR);
        printf("Requesting read...\n");
        int ret = read(fd, out, 5);
        printf("Done reading.\n");
        printf("Message retrieved: %s\n", out);
    }
    else {
        sleep(4);
        fd = open("/dev/dm510-1", O_RDWR);

        printf("Writing message: %s\n", msg);
        int ret = write(fd, msg, 5);
        printf("Done writing.\n");
    }
}