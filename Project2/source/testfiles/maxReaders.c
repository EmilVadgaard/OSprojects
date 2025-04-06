#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>  // for _IO, _IOW, _IOR macros

#define DM_IOC_MAGIC 'e'
#define DM_IOCSBUFFER _IOW(DM_IOC_MAGIC, 1, int)  // Set buffer size -> write int
#define DM_IOCGBUFFER _IOR(DM_IOC_MAGIC, 2, int)  // Get buffer size -> read int
#define DM_IOCSMAXREAD _IOW(DM_IOC_MAGIC, 3, int) 
#define DM_IOCGMAXREAD _IOR(DM_IOC_MAGIC, 4, int) 

int main(int argc, char *argv[])
{
    int fd;

    int maxReaders;
    int new_maxReaders = 2;

    char out[20];

    fd = open("/dev/dm510-0", O_RDWR);

    if (ioctl(fd, DM_IOCGMAXREAD, &maxReaders) < 0){
        close(fd);
        return -1;
    }

    printf("Max Readers: %d\n", maxReaders);

    if (ioctl(fd, DM_IOCSMAXREAD, &new_maxReaders) < 0){
        close(fd);
        return -1;
    }

    if (ioctl(fd, DM_IOCGMAXREAD, &maxReaders) < 0){
        close(fd);
        return -1;
    }

    printf("Max Readers: %d\n", maxReaders);

    close(fd);

    pid_t pid1, pid2, pid3;

    pid1 = fork();
    if (pid1 == 0){
        printf("Process 1 opened.\n");
        fd = open("/dev/dm510-0", O_RDWR);

        int ret = read(fd, out, 5);

        if (ret < 0){
            printf("Process 1 exceeded max readers.\n");
        }
        else {
            printf("Process 1 succesfully read.\n");
        }

        return 0;
    }

    pid2 = fork();
    if (pid2 == 0){
        printf("Process 2 opened.\n");
        fd = open("/dev/dm510-0", O_RDWR);

        int ret = read(fd, out, 5);

        if (ret < 0){
            printf("Process 2 exceeded max readers.\n");
        }
        else {
            printf("Process 2 succesfully read.\n");
        }

        return 0;
    }

    pid3 = fork();
    if (pid3 == 0){
        printf("Process 3 opened.\n");
        fd = open("/dev/dm510-0", O_RDWR);

        int ret = read(fd, out, 5);

        if (ret < 0){
            printf("Process 3 exceeded max readers.\n");
        }
        else {
            printf("Process 3 succesfully read.\n");
        }
        
        return 0;
    }

    sleep(2);

    char msg[] = "hello";

    fd = open("/dev/dm510-1", O_RDWR);
    write(fd, msg, 5);
    write(fd, msg, 5);

}
