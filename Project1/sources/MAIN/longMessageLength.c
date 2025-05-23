#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../arch/x86/include/generated/uapi/asm/unistd_64.h"

int main(int argc, char ** argv) {
    char *in = "This is an example message.";
    char msg[50];
    int err;

    printf("putting in this message below\n%s\n\n", in);
    /* Send a message containing 'in' */
    err = syscall(__NR_dm510_msgbox_put, in, strlen(in)+5);
    if (err < 0){
        errno = -err;
        perror("put_msg:");
    }

    /* Read a message */
    printf("Below is retrieved message:\n");
    err = syscall(__NR_dm510_msgbox_get, msg, 50);
    if (err < 0){
        errno = -err;
        perror("get_msg:");
    }
    printf("%s\n", msg);

    return 0;
}
