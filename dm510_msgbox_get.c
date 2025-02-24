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
int sys_dm510_msgbox_get( char* buffer , int length ) {
    if (top != NULL) {
        msg_t* msg = top;
        int mlength = msg->length;
        top = msg->previous;
    
        if (length < mlength) {
            return -1;
        }
    
        /* copy message */
        copy_to_user(buffer, msg->message, mlength);
    
        /* free memory */
        kfree(msg->message);
        kfree(msg);
    
        return mlength;
    }
return -1;
}