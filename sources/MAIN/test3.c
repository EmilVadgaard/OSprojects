#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../arch/x86/include/generated/uapi/asm/unistd_64.h"

static void checkError(int errorCode){
    if (errorCode < 0){
        errno = -errorCode;
        perror("Error: ");
    }
}


int main(int argc, char ** argv) {
    printf("Calling ... \n");
    char *one = "1st message";
    char *two = "2nd message";
    char *three = "3rd message";
    char msg[50];
    int err;

    printf("putting in these messages below\n%s\n%s\n%s\n\n", one, two, three);
    /* Send a message containing 'in' */

    printf("Putting first message...");
    err = syscall(__NR_dm510_msgbox_put, one, strlen(one)+1);
    checkError(err);
    err = syscall(__NR_dm510_msgbox_get, msg, 50);
    checkError(err);
    printf("Retrieving...\n%s\n",msg);

    printf("Putting first message...");
    err = syscall(__NR_dm510_msgbox_put, two, strlen(two)+1);
    checkError(err);
    err = syscall(__NR_dm510_msgbox_get, msg, 50);
    checkError(err);
    printf("Retrieving...\n%s\n",msg);

    printf("Putting first message...");
    err = syscall(__NR_dm510_msgbox_put, three, strlen(three)+1);
    checkError(err);
    err = syscall(__NR_dm510_msgbox_get, msg, 50);
    checkError(err);
    printf("Retrieving...\n%s\n",msg);


    return 0;
}

