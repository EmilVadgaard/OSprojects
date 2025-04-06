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
    int fd1;
    int fd2;

    int buffersize1;
    int buffersize2;
    int new_buffer = 2000;

    fd1 = open("/dev/dm510-0", O_RDWR);

    if (ioctl(fd1, DM_IOCGBUFFER, &buffersize1) < 0){
        close(fd1);
        return -1;
    }

    printf("Buffersize: %d\n", buffersize1);

    if (ioctl(fd1, DM_IOCSBUFFER, &new_buffer) < 0){
        close(fd1);
        return -1;
    }

    if (ioctl(fd1, DM_IOCGBUFFER, &buffersize1) < 0){
        close(fd1);
        return -1;
    }

    printf("Buffersize for dm510-0: %d\n", buffersize1);

    fd2 = open("/dev/dm510-1", O_RDWR);

    if (ioctl(fd2, DM_IOCGBUFFER, &buffersize2) < 0){
        close(fd2);
        return -1;
    }

    printf("Buffersize for dm510-1: %d\n", buffersize2);

    close(fd1);
    close(fd2);

}
