#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "../arch/x86/include/generated/uapi/asm/unistd_64.h"

int main(int argc, char ** argv) {
    printf("Calling ... \n");
    char *one = "1st message";
    char *two = "2nd message";
    char *three = "3rd message";
    char msg[50];
    int msglen;

    printf("putting in these messages below\n%s\n%s\n%s\n\n", one, two, three);
    /* Send a message containing 'in' */
    syscall(__NR_dm510_msgbox_put, one, strlen(one)+1);
    syscall(__NR_dm510_msgbox_put, two, strlen(two)+1);
    syscall(__NR_dm510_msgbox_put, three, strlen(three)+1);

    /* Read a message */
    printf("Below is retrieved message:\n");

    msglen = syscall(__NR_dm510_msgbox_get, msg, 50);
    printf("%s\n",msg);
    msglen = syscall(__NR_dm510_msgbox_get, msg, 50);
    printf("%s\n",msg);
    msglen = syscall(__NR_dm510_msgbox_get, msg, 50);
    printf("%s\n",msg);


    return 0;
}