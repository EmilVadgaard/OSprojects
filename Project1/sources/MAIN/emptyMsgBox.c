#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../arch/x86/include/generated/uapi/asm/unistd_64.h"

int main(int argc, char ** argv) {
    printf("Calling ... \n");
    char *in = "This is an example message.";
    char msg[50];
    int err;

    printf("putting in no message.\n");

    /* Read a message */
    printf("Below is retrieved message:\n%s\n", msg);
    err = syscall(__NR_dm510_msgbox_get, msg, 50);
    if (err < 0){
        errno = -err;
        perror("get_msg:");
    }
        
    return 0;
}