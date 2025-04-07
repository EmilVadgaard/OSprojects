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

#define DM_BUFFER 10
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
	wait_queue_head_t inq, outq;       // Read and Write queues
	char *buffer, *end;                // Beginning of buffer, end of buffer
	int buffersize;                    // Pre-Calculation of buffer
	char *rp, *wp;                     // Read & Write Start Pointer
	int nreaders, nwriters;            // Number of readers & writers
	struct mutex mutex;                // Mutual exclusion semaphore
	struct cdev cdev;                  // Character device structure
	struct DM510_pipe *other_pipe;     // Reference to other device
};

static struct DM510_pipe *dm_devices;

dev_t dm_devno;

static int max_readers = 0;

static void DM510_cdev_setup(struct DM510_pipe *dm_device, int index ){
		int err, devno = dm_devno + index;
		
		cdev_init(&dm_device->cdev, &dm510_fops);
		dm_device->cdev.owner = THIS_MODULE;
		err = cdev_add (&dm_device->cdev, devno, 1);
		/*  Fail gracefully if need be  */
		if (err){
			printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
		}
			
	}

// When we run ./dm510_load
int dm510_init_module( void ) {
	dev_t firstdev = MKDEV(MAJOR_NUMBER, MIN_MINOR_NUMBER);

	int err = register_chrdev_region(firstdev, DEVICE_COUNT, DEVICE_NAME);	// Creating 2 devices
	if (err < 0){
		return err;
	}

	dm_devno = firstdev;
	dm_devices = kmalloc(DEVICE_COUNT * sizeof(struct DM510_pipe), GFP_KERNEL);

	if (dm_devices == NULL) {						// If memory allocation wasn't possible
		unregister_chrdev_region(firstdev, DEVICE_COUNT);	
		return -EBUSY;
	}

	memset(dm_devices, 0, DEVICE_COUNT * sizeof(struct DM510_pipe));

	for (int i = 0; i < DEVICE_COUNT; i++) {				// Init for each device
		init_waitqueue_head(&(dm_devices[i].inq));
		init_waitqueue_head(&(dm_devices[i].outq));
		mutex_init(&dm_devices[i].mutex);
		DM510_cdev_setup(dm_devices + i, i);

		dm_devices[i].buffer = kmalloc(DM_BUFFER, GFP_KERNEL);
		if (!dm_devices[i].buffer) {
			cdev_del(&dm_devices[i].cdev);
			unregister_chrdev_region(firstdev, DEVICE_COUNT);
			kfree(dm_devices);
			return -ENOMEM;
		}
		dm_devices[i].buffersize = DM_BUFFER;
		dm_devices[i].end = dm_devices[i].buffer + dm_devices[i].buffersize;
		dm_devices[i].rp = dm_devices[i].wp = dm_devices[i].buffer;
	}

	/*  Reference to each-other  */
	dm_devices[0].other_pipe = &dm_devices[1];
	dm_devices[1].other_pipe = &dm_devices[0];

	printk(KERN_INFO "DM510: Hello from your device!\n");
	return 0;
}

// When we run ./dm510_unload
void dm510_cleanup_module( void ) {

	if (!dm_devices) 							// If devices already does not exist
		return; 

	for (int i = 0; i < DEVICE_COUNT; i++) {				// Deleting devices individually
		cdev_del(&dm_devices[i].cdev);
		kfree(dm_devices[i].buffer);
	}
	kfree(dm_devices);
	unregister_chrdev_region(dm_devno, DEVICE_COUNT);
	dm_devices = NULL; /* pedantic */

	printk(KERN_INFO "DM510: Module unloaded.\n");
}


/* Called when a process tries to open the device file */
static int dm510_open( struct inode *inode, struct file *filp ) {
	struct DM510_pipe *dev;

	dev = container_of(inode->i_cdev, struct DM510_pipe, cdev);
	filp->private_data = dev;

	if (mutex_lock_interruptible(&dev->mutex)) 				// Aquire device, it is now locked
		return -ERESTARTSYS;

	if (max_readers && dev->nreaders >= max_readers){			// Ensure not too many current readers
		mutex_unlock(&dev->mutex);
		return -EBUSY;
	}

	if (filp->f_mode & FMODE_READ)			// If process wants to be reader
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)			// If process wants to be writer
		dev->nwriters++;
	
	mutex_unlock(&dev->mutex);						// Release lock

	return nonseekable_open(inode, filp);
}


/* Called when a process closes the device file. */
static int dm510_release( struct inode *inode, struct file *filp ) {
	struct DM510_pipe *dev = filp->private_data;

	mutex_lock(&dev->mutex);						// Aquire device, it is now locked.
	
	if (filp->f_mode & FMODE_READ)			// If process was reader
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)			// If process was writer
		dev->nwriters--;

	mutex_unlock(&dev->mutex);						// Release lock
	return 0;
		
}


/* Called when a process, which already opened the dev file, attempts to read from it. */
static ssize_t dm510_read( struct file *filp,
    char *ret_buf,      // Buffer which will be filled
    size_t count,   	// Max bytes worth, being read
    loff_t *f_pos )  	// File offset
{
	struct DM510_pipe *dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->mutex))				// Aquire device, it is now locked
		return -ERESTARTSYS;

	/*  If data does not exist  */
	while (dev->rp == dev->wp) { 						// Nothing to read in buffer
		mutex_unlock(&dev->mutex);						// Release lock, to allow others
		if (filp->f_flags & O_NONBLOCK)	
			return -EAGAIN;	
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))		// Sleep, until writers notify that data is on buffer
			return -ERESTARTSYS;	
		if (mutex_lock_interruptible(&dev->mutex))				// When awakened, aquire lock, and check
			return -ERESTARTSYS;
	}

	/*  Data exists in buffer  */
	if (dev->wp > dev->rp)							// In case we do not read past edge
		count = min(count, (size_t)(dev->wp - dev->rp));
	else 									// In case we do read past edge, wrapping case
		count = min(count, (size_t)(dev->end - dev->rp));
	
	if (copy_to_user(ret_buf, dev->rp, count)) {				// Give to user-space amount of bytes written
		mutex_unlock (&dev->mutex);
		return -EFAULT;
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	mutex_unlock (&dev->mutex);						// Release lock

	wake_up_interruptible(&dev->outq);					// Awaken writers who are waiting on space on buffer
	
	return count;
}

static int spacefree(struct DM510_pipe *dev)
{
	if (dev->rp == dev->wp)							// Empty buffer					
		return dev->buffersize - 1;
	return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;	// Distance between Read pointer & Write pointer across edge
}

static int dm_getwritespace(struct DM510_pipe *dev, struct file *filp)
{
	while (spacefree(dev) == 0) { /* full */
		mutex_unlock(&dev->mutex);					// Aquire device, it is now locked

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		
		if (wait_event_interruptible(dev->outq, spacefree(dev) > 0))	// Sleep, until readers 
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&dev->mutex))			// Release lock
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
	struct DM510_pipe *real_dev = dev->other_pipe;				// Write inside the other device
	int result;

	if (mutex_lock_interruptible(&real_dev->mutex))				// Aquire device, it is now locked
		return -ERESTARTSYS;

	result = dm_getwritespace(real_dev, filp);				// Ensure there is space in buffer
	if (result)
		return result;

	/*  Space in buffer, write into buffer  */
	count = min(count, (size_t)spacefree(real_dev));
	if (real_dev->wp >= real_dev->rp)					// In case we do not write past edge
		count = min(count, (size_t)(real_dev->end - real_dev->wp));
	else									// In case we do write past edge, wrapping.
		count = min(count, (size_t)(real_dev->rp - real_dev->wp - 1));
	if (copy_from_user(real_dev->wp, buf, count)) {				// Take data input from user-space and write into buffer
		mutex_unlock (&real_dev->mutex);
		return -EFAULT;
	}
	real_dev->wp += count;
	if (real_dev->wp == real_dev->end)
		real_dev->wp = real_dev->buffer;					// Wrapped
	mutex_unlock(&real_dev->mutex);						// Release lock
	
	wake_up_interruptible(&real_dev->inq);					// Awaken readers waiting on new data

	return count;
}

static long set_buffer(struct DM510_pipe *dev, unsigned long arg){
	int new_size;

	if (copy_from_user(&new_size, (int __user *)arg, sizeof(new_size))) {	// Receive new size from user-space
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&dev->mutex))				// Aquire device, it is now locked
            return -ERESTARTSYS;

	if (dev->buffer) {							// Free old buffer
		kfree(dev->buffer);
		dev->buffer = NULL;
	}

	/*  Creating new buffer  */
	dev->buffer = kmalloc(new_size, GFP_KERNEL);
	if (!dev->buffer) {
		mutex_unlock(&dev->mutex);
		printk(KERN_ERR "dm510: failed to allocate new buffer of size=%d\n", new_size);
		return -ENOMEM;
	}

	dev->buffersize = new_size;
    dev->end = dev->buffer + new_size;
    dev->rp = dev->wp = dev->buffer;

    mutex_unlock(&dev->mutex);							// Release lock

	wake_up_interruptible(&dev->outq);					// Awaken any waiting writers, as buffer is fully empty

    printk(KERN_INFO "dm510: buffer resized to %d\n", new_size);

	return 0;
}

static long set_max_readers(struct DM510_pipe *dev, unsigned long arg){
	int new_cap;

	if (copy_from_user(&new_cap, (int __user *)arg, sizeof(new_cap))) {	// Receive new number from user-space
		return -EFAULT;
	}

	if(mutex_lock_interruptible(&dev->mutex))				// Aquire device, it is now locked
		return -ERESTARTSYS;

	max_readers = new_cap;

	mutex_unlock(&dev->mutex);						// Release lock

	return 0;
}

/* called by system call icotl */ 
long dm510_ioctl( 
    struct file *filp, 
    unsigned int cmd,   	// Command
    unsigned long arg ) 	// Arguments for command
{
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
