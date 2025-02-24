#include "linux/kernel.h"
#include "linux/unistd.h"
#include "linux/slab.h"
#include "linux/uaccess.h"

typedef struct _msg_t msg_t;

struct _msg_t{
  msg_t* previous;
  int length;
  char* message;
};

static msg_t *top = NULL;

asmlinkage
int sys_dm510_msgbox_put( char* buffer , int length ) {
    msg_t* msg = kmalloc(sizeof(msg_t), GFP_KERNEL);
    if (msg == NULL) return -1;
    msg->previous = NULL;
    msg->length = length;
    msg->message = kmalloc(length, GFP_KERNEL);
    if (msg->message == NULL) return -2;
    copy_from_user(msg->message, buffer, length);
    if (msg->message != buffer) return -3;

    if (top == NULL) {
        top = msg;
    } else {
        /* not empty stack */
        msg->previous = top;
        top = msg;
    }
return 0;
}