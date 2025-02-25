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
    //printk("this is from kernel memory: %s\n", msg->message);

    if (top == NULL) {
        top = msg;
    } else {
        /* not empty stack */
        msg->previous = top;
        top = msg;
    }
return 0;
}

asmlinkage
int sys_dm510_msgbox_get( char* buffer , int length ) {
    if (top != NULL) {
        msg_t* msg = top;
        int mlength = msg->length;
        top = msg->previous;
    
        if (length < mlength) {
            return -1;
        }
        
        //printk("from get function: %s",msg->message);
    
        /* copy message */
        copy_to_user(buffer, msg->message, mlength);
    
        /* free memory */
        kfree(msg->message);
        kfree(msg);
    
        return mlength;
    }
return -1;
}