#include "linux/kernel.h"
#include "linux/unistd.h"
#include "linux/slab.h"
#include "linux/uaccess.h"
#include "linux/errno.h"
#include "linux/irqflags.h"

typedef struct _msg_t msg_t;

struct _msg_t{
  msg_t* previous;
  int length;
  char* message;
};

static msg_t *top = NULL;

unsigned long flags;

asmlinkage
int sys_dm510_msgbox_put( char* buffer , int length ) {
    if (length < 0 || buffer == NULL) return -EINVAL;
    msg_t* msg = kmalloc(sizeof(msg_t), GFP_KERNEL);
    if (msg == NULL) return -EFAULT;  

    msg->previous = NULL;
    msg->length = length;
    msg->message = kmalloc(length, GFP_KERNEL);
    if (msg->message == NULL) return -EFAULT;

    int err = copy_from_user(msg->message, buffer, length);
    if (err != 0) return -EFAULT;

    local_irq_save(flags);
    /* CRITICAL SECTION */
    if (top == NULL) {
        top = msg;
    } else {
        /* not empty stack */
        msg->previous = top;
        top = msg;
    }
    /* DONESKI */
    local_irq_restore(flags);
    return 0;
}

asmlinkage
int sys_dm510_msgbox_get( char* buffer , int length ) {
    if (buffer == NULL) return -EINVAL; // FIX
    if (top != NULL) {
        local_irq_save(flags);
        /* CRITICAL SECTION */
        msg_t* msg = top;
        int mlength = msg->length;
    
        if (length < mlength) {
            return -EINVAL;
        }
    
        /* copy message */
        int err = copy_to_user(buffer, msg->message, mlength);
        if (err != 0) return -EFAULT;
        top = msg->previous;
        /* DONESKI */
        local_irq_restore(flags);
        
        /* free memory */
        kfree(msg->message);
        kfree(msg);
        
    
        return mlength;
    }
return -ENODATA;
}