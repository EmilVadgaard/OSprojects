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

    //fork 3-4 processor, som alle læser., efter der er skrevet rigtig meget??

}
