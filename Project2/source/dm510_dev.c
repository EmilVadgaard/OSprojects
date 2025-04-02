/* Prototype module for second mandatory DM510 assignment */
#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>	
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/cdev.h>
/* #include <asm/uaccess.h> */
#include <linux/uaccess.h>
#include <linux/semaphore.h>
/* #include <asm/system.h> */
#include <asm/switch_to.h>
/* Prototypes - this would normally go in a .h file */
static int dm510_open( struct inode*, struct file* );
static int dm510_release( struct inode*, struct file* );
static ssize_t dm510_read( struct file*, char*, size_t, loff_t* );
static ssize_t dm510_write( struct file*, const char*, size_t, loff_t* );
long dm510_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

#define DEVICE_NAME "dm510_dev" /* Dev name as it appears in /proc/devices */
#define MAJOR_NUMBER 255
#define MIN_MINOR_NUMBER 0
#define MAX_MINOR_NUMBER 1

#define DEVICE_COUNT 2

#define DM_BUFFER 1000
/* end of what really should have been in a .h file */

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */

#define DM510_IOC_MAGIC 'e'

#define DM_IOCSBUFFER _IOW(DM510_IOC_MAGIC, 1, int)
#define DM_IOCGBUFFER _IOR(DM510_IOC_MAGIC, 2, int)
#define DM_IOCSMAXREAD _IOW(DM510_IOC_MAGIC, 3, int)
#define DM_IOCGMAXREAD _IOR(DM510_IOC_MAGIC, 4, int)

/* file operations struct */
static struct file_operations dm510_fops = {
	.owner   = THIS_MODULE,
	.read    = dm510_read,
	.write   = dm510_write,
	.open    = dm510_open,
	.release = dm510_release,
    .unlocked_ioctl   = dm510_ioctl
};

struct DM510_pipe {
	wait_queue_head_t inq, outq;       /* read and write queues */
	char *buffer, *end;                /* begin of buf, end of buf */
	int buffersize;                    /* used in pointer arithmetic */
	char *rp, *wp;                     /* where to read, where to write */
	int nreaders, nwriters;            /* number of openings for r/w */
	struct mutex mutex;                /* mutual exclusion semaphore */
	struct cdev cdev;                  /* Char device structure */
	struct DM510_pipe *other_pipe;
};

static struct DM510_pipe *dm_devices;

dev_t dm_devno;

static int max_readers = 0;

static void DM510_cdev_setup(struct DM510_pipe *dm_device, int index ){
		int err, devno = dm_devno + index;
		
		cdev_init(&dm_device->cdev, &dm510_fops);
		dm_device->cdev.owner = THIS_MODULE;
		err = cdev_add (&dm_device->cdev, devno, 1);
		/* Fail gracefully if need be */
		if (err)
			printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
			//return -1; error handling
	}

/* called when module is loaded */
int dm510_init_module( void ) {
	dev_t firstdev = MKDEV(MAJOR_NUMBER, MIN_MINOR_NUMBER);

	int err = register_chrdev_region(firstdev, DEVICE_COUNT, DEVICE_NAME);
	if (err != 0){
		//Shit error handling here
	}

	dm_devno = firstdev;
	dm_devices = kmalloc(DEVICE_COUNT * sizeof(struct DM510_pipe), GFP_KERNEL);

	if (dm_devices == NULL) {
		unregister_chrdev_region(firstdev, DEVICE_COUNT);
		return -1;
	}

	memset(dm_devices, 0, DEVICE_COUNT * sizeof(struct DM510_pipe));

	for (int i = 0; i < DEVICE_COUNT; i++) {
		init_waitqueue_head(&(dm_devices[i].inq));
		init_waitqueue_head(&(dm_devices[i].outq));
		mutex_init(&dm_devices[i].mutex);
		DM510_cdev_setup(dm_devices + i, i);

		dm_devices[i].buffer = kmalloc(DM_BUFFER, GFP_KERNEL);
		if (!dm_devices[i].buffer) return -ENOMEM;
		dm_devices[i].buffersize = DM_BUFFER;
		dm_devices[i].end = dm_devices[i].buffer + dm_devices[i].buffersize;
		dm_devices[i].rp = dm_devices[i].wp = dm_devices[i].buffer;
	}
	dm_devices[0].other_pipe = &dm_devices[1];
	dm_devices[1].other_pipe = &dm_devices[0];

	/* initialization code belongs here */

	printk(KERN_INFO "DM510: Hello from your device!\n");
	return 0;
}

/* Called when module is unloaded */
void dm510_cleanup_module( void ) {

	if (!dm_devices)
		return; /* nothing else to release */

	for (int i = 0; i < DEVICE_COUNT; i++) {
		cdev_del(&dm_devices[i].cdev);
		kfree(dm_devices[i].buffer);
	}
	kfree(dm_devices);
	unregister_chrdev_region(dm_devno, DEVICE_COUNT);
	dm_devices = NULL; /* pedantic */

	/* clean up code belongs here */

	printk(KERN_INFO "DM510: Module unloaded.\n");
}


/* Called when a process tries to open the device file */
static int dm510_open( struct inode *inode, struct file *filp ) {
	struct DM510_pipe *dev;

	dev = container_of(inode->i_cdev, struct DM510_pipe, cdev);
	filp->private_data = dev;

	if (mutex_lock_interruptible(&dev->mutex)) //We aquire device, it is now locked.
		return -ERESTARTSYS;
	
	//Now we have allocated memeory for messages.
	 /* rd and wr from the beginning */

	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters++;
	mutex_unlock(&dev->mutex);

	return nonseekable_open(inode, filp);
	/* device claiming code belongs here */

	return 0;
}


/* Called when a process closes the device file. */
static int dm510_release( struct inode *inode, struct file *filp ) {

	struct DM510_pipe *dev = filp->private_data;

	/* remove this filp from the asynchronously notified filp's */
	mutex_lock(&dev->mutex);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
//	if (dev->nreaders + dev->nwriters == 0) {
//		kfree(dev->buffer);
//		dev->buffer = NULL; /* the other fields are not checked on open */
//	}
	mutex_unlock(&dev->mutex);
	return 0;
		
}


/* Called when a process, which already opened the dev file, attempts to read from it. */
static ssize_t dm510_read( struct file *filp,
    char *ret_buf,      /* The buffer to fill with data     */
    size_t count,   /* The max number of bytes to read  */
    loff_t *f_pos )  /* The offset in the file           */
{
	struct DM510_pipe *dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	if (max_readers && dev->nreaders >= max_readers){
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	while (dev->rp == dev->wp) { /* nothing to read */
		mutex_unlock(&dev->mutex); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		//Erstat med andet?  PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(dev->end - dev->rp));
	if (copy_to_user(ret_buf, dev->rp, count)) {
		mutex_unlock (&dev->mutex);
		return -EFAULT;
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	mutex_unlock (&dev->mutex);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	//Erstat igen med andet?  PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
	return count;
	/* read code belongs here */
	
}

static int spacefree(struct DM510_pipe *dev)
{
	if (dev->rp == dev->wp)
		return dev->buffersize - 1;
	return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

static int dm_getwritespace(struct DM510_pipe *dev, struct file *filp)
{
	while (spacefree(dev) == 0) { /* full */
		DEFINE_WAIT(wait);
		
		mutex_unlock(&dev->mutex);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		//Erstat!!  PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		if (signal_pending(current))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		if (mutex_lock_interruptible(&dev->mutex))
			return -ERESTARTSYS;
	}
	return 0;
}	



/* Called when a process writes to dev file */
static ssize_t dm510_write( struct file *filp,
    const char *buf,/* The buffer to get data from      */
    size_t count,   /* The max number of bytes to write */
    loff_t *f_pos )  /* The offset in the file           */
{
	struct DM510_pipe *dev = filp->private_data;
	struct DM510_pipe *real_dev = dev->other_pipe;
	int result;

	if (mutex_lock_interruptible(&real_dev->mutex))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	result = dm_getwritespace(real_dev, filp);
	if (result)
		return result; /* scull_getwritespace called mutex_unlock(&dev->mutex) */

	/* ok, space is there, accept something */
	count = min(count, (size_t)spacefree(real_dev));
	if (real_dev->wp >= real_dev->rp)
		count = min(count, (size_t)(real_dev->end - real_dev->wp)); /* to end-of-buf */
	else /* the write pointer has wrapped, fill up to rp-1 */
		count = min(count, (size_t)(real_dev->rp - real_dev->wp - 1));
	//Erstat!!  PDEBUG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
	if (copy_from_user(real_dev->wp, buf, count)) {
		mutex_unlock (&real_dev->mutex);
		return -EFAULT;
	}
	real_dev->wp += count;
	if (real_dev->wp == real_dev->end)
		real_dev->wp = real_dev->buffer; /* wrapped */
	mutex_unlock(&real_dev->mutex);

	/* finally, awake any reader */
	wake_up_interruptible(&real_dev->inq);  /* blocked in read() and select() */

	return count;
}

static long set_buffer(struct DM510_pipe *dev, unsigned long arg){
	//check om der er argument

	int new_size;

	if (copy_from_user(&new_size, (int __user *)arg, sizeof(new_size))) {
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&dev->mutex))
            return -ERESTARTSYS;

	if (dev->buffer) {
		kfree(dev->buffer);
		dev->buffer = NULL;
	}

	dev->buffer = kmalloc(new_size, GFP_KERNEL);
	if (!dev->buffer) {
		mutex_unlock(&dev->mutex);
		printk(KERN_ERR "dm510: failed to allocate new buffer of size=%d\n", new_size);
		return -ENOMEM;
	}

	dev->buffersize = new_size;
    dev->end = dev->buffer + new_size;
    dev->rp = dev->wp = dev->buffer;

    mutex_unlock(&dev->mutex);

    printk(KERN_INFO "dm510: buffer resized to %d\n", new_size);

	return 0;
}

static long set_max_readers(struct DM510_pipe *dev, unsigned long arg){
	//flere overvejesler
	// feks mere en max readers allerede.
	int new_cap;

	if (copy_from_user(&new_cap, (int __user *)arg, sizeof(new_cap))) {
		return -EFAULT;
	}

	if(mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	max_readers = new_cap;

	mutex_unlock(&dev->mutex);

	return 0;
}

/* called by system call icotl */ 
long dm510_ioctl( 
    struct file *filp, 
    unsigned int cmd,   /* command passed from the user */
    unsigned long arg ) /* argument of the command */
{
	/* ioctl code belongs here */
	struct DM510_pipe *dev = filp->private_data;
	int err;

	switch (cmd) {
		case DM_IOCSBUFFER:
			err = set_buffer(dev, arg);
			if (err) return err;
			break;

		case DM_IOCGBUFFER:
			int size = dev->buffersize;
			if (copy_to_user((int __user *)arg, &size, sizeof(size)))
				return -EFAULT;
			break;

		case DM_IOCSMAXREAD:
			err = set_max_readers(dev, arg);
			if (err) return err;
			break;

		case DM_IOCGMAXREAD:
			if (copy_to_user((int __user *)arg, &max_readers, sizeof(max_readers)))
				return -EFAULT;
			break;

		default:
			return -EINVAL;
	}

	printk(KERN_INFO "DM510: ioctl called.\n");

	return 0; //has to be changed
}

module_init( dm510_init_module );
module_exit( dm510_cleanup_module );

MODULE_AUTHOR( "...Your names here. Do not delete the three dots in the beginning." );
MODULE_LICENSE( "GPL" );
