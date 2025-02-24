#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../arch/x86/include/generated/uapi/asm/unistd_64.h"

int main(int argc, char ** argv) {
    printf("Calling ... \n");
    char *in = "This is an example message.";
    char msg[50];
    int msglen;

    printf("putting in this message below\n%s\n", in);
    /* Send a message containing 'in' */
    syscall(__NR_dm510_msgbox_put, in, strlen(in)+1);

    printf("Retrieving message\n");
    /* Read a message */
    msglen = syscall(__NR_dm510_msgbox_get, msg, 50);

    printf("Below is retriueved message:\n%s\n", msg);

    return 0;
}