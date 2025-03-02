#include "linux/kernel.h"
#include "linux/unistd.h"

asmlinkage
int sys_hellokernel( int flag ) {
    printk("Your kernel greets you %d times! \n", flag);
return 0;
}