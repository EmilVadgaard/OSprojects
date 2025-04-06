#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int device0;
    int device1;

    char msg1[] = "HelloWorld";
    char msg2[] = "01234567";
    char out1[20];
    char out2[20];

    pid_t pid;

    pid = fork();

    if (pid == 0){
        device0 = open("/dev/dm510-0", O_RDWR);
        int ret = write(device0, msg1, 10);
        printf("put message: %s\n", msg1);
    
    
        ret = write(device0, msg2, 9);
        printf("put message: %s\n", msg2);

        ret = write(device0, msg2, 9);
        printf("put message: %s\n", msg2);

        return 0;
    }
    else {
        sleep(4);
        device1 = open("/dev/dm510-1", O_RDWR);

        int ret = read(device1, out1, 11);
        printf("retrieved message: %s\n", out1);

        ret = read(device1, out2, 9);
        printf("retrieved message: %s\n", out2);

        ret = read(device1, out2, 8);
        printf("retrieved message: %s\n", out2);

        return 0;
    }

    return 0;
}
