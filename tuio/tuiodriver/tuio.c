/* 
 *
 * Name: 
 *    tuio
 *
 * Author:
 *    Pavan Trikutam, Ian Stewart, Henry Phan
 *
 * Description:
 *    This is the device that will accept and output TUIO information through
 *    its read/write functions. The device will only accept one reader and one
 *    writer at a time. If the device is already being used -EBUSY is returned.
 *    A read requires a buffer of sufficient length to include the entire data.
 *    No partial messages are returned, -EINVAL if buffer length unacceptable.
 *
 *    The device currently buffers up to BUF_COUNT messages of length BUF_LEN.
 *    A message is read only once, than is no longer available. If a message is
 *    not read before BUF_COUNT messages have been written, following writes 
 *    will overide old messages.
 *
 *
 * Usage:
 *    Use through unbuffered read/writes to the device file.
 *
 *
 * Note: the /dev/hello_world tutorial on linuxdevcenter.com was used as a
 * starting point for this device file.
 * 
 * Modified:
 * 	05/08/2009	Pavan	Original Write
 * 	05/22/2009  Ian   Modified for tuio use
 * 	05/26/2009  Ian   Modified read to block for new data
 * 	05/27/2009  Ian   Added a ring buffer
 * 	05/27/2009  Ian   Converted from tsdev to tuio
 */


#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <asm/uaccess.h>

#define BUF_COUNT 20    /* Number of buffers in the ring */
#define BUF_LEN 256     /* Maximum message length */
#define DEV_NAME "tuio" /* Device filename: /dev/DEV_NAME */


/* Wait queue for reading */
static DECLARE_WAIT_QUEUE_HEAD(tuio_read_wait);

/* Ring buffer entry struct */
struct buf_ring_ent {
   char buf[BUF_LEN];   /* Actual data in the buffer */
   size_t len;          /* Length of data in the buffer */
   struct buf_ring_ent *next; /* Pointer to next entry in ring */
};

static struct buf_ring_ent *read_buf;  /* Pointer to the current read buffer */
static struct buf_ring_ent *write_buf; /* Pointer to the current write buffer */

static int read_busy = 0;  /* Used to prevent multiple readers */
static int write_busy = 0; /* Used to prevent multiple writers */


/*
 * A current read will return the next available data. If no new data is
 * available, the thread will block and wait for new data.
 *
 * If O_NONBLOCK is set the read will return -EAGAIN 
 *
 * The offset parameter 'loff_t*' is never modified and as a result no end of
 * file will ever be reached.
 */
static ssize_t tuio_read(struct file * file, char * buf, 
			  size_t count, loff_t *ppos)
{
   ssize_t retval;

   /* Sleep until new data has arrived */
   while (!read_buf->len) {

      if (file->f_flags & O_NONBLOCK)
         return -EAGAIN;

      /* Wait until there is data in the current read buffer */
      wait_event_interruptible(tuio_read_wait, read_buf->len != 0);

      if (signal_pending(current))
         return -ERESTARTSYS;
   }

   /* Only support full buffer reads */
   if (count < read_buf->len)
      return -EINVAL;

	/*
	 * Besides copying the string to the user provided buffer,
	 * this function also checks that the user has permission to
	 * write to the buffer, that it is mapped, etc.
	 */
   if (copy_to_user(buf, read_buf->buf, read_buf->len))
      return -EINVAL;

   /* Clear the current buffer and move on */
   retval = read_buf->len;
   read_buf->len = 0;
   read_buf = read_buf->next;

	/* All data has been passed. */
	return retval;
}

/*
 * Writes the message into the device. Message may be only a maximum length of
 * BUF_LEN; -EINVAL is returned otherwise.
 */
static ssize_t tuio_write(struct file * file, const char * buf, 
			size_t count, loff_t * offp)
{
   /* verify the length of the data */
   if (count > BUF_LEN)
      return -EINVAL;

   /* If we are overwritting data,
    * move the read pointer to preserve message order */
   if (write_buf->len) {
      read_buf = read_buf->next;
   }

	/* Get the data and store it. */
	if (copy_from_user(write_buf->buf,buf,count))
		return -EINVAL;

   /* Save the data length */
   write_buf->len = count;

   /* Move the current ring pointer */
   write_buf = write_buf->next;

   /* Wakeup the blocking threads */
   wake_up_interruptible(&tuio_read_wait);

	return count;
}

/*
 * Called when a process tries to open the device file. Only single reader
 * and single writer currently accepted.
 */
static int tuio_open(struct inode *inode, struct file *file)
{
   /* Determine what find of user this file is */
   if (file->f_mode & FMODE_READ) {
      if (read_busy)
         return -EBUSY;
      read_busy++;
   }
   if(file->f_mode & FMODE_WRITE) {
      if (write_busy)
         return -EBUSY;
      write_busy++;
   }

   /* only support read or write */
   if (!(file->f_mode & FMODE_READ || file->f_mode & FMODE_WRITE))
      return -EINVAL;


   try_module_get(THIS_MODULE);

   return 0;
}

/*
 * Called when a process closes the device file
 */
static int tuio_release(struct inode *indoe, struct file *file)
{
   if (file->f_mode & FMODE_READ)
      read_busy--;
   if (file->f_mode & FMODE_WRITE)
      write_busy--;

   module_put(THIS_MODULE);

   return 0;
}


/*
 * The only file operations we care about are read, write, open, and release.
 */
static const struct file_operations tuio_fops = {
	.owner   = THIS_MODULE,
	.read    = tuio_read,
	.write   = tuio_write,
   .open    = tuio_open,
   .release = tuio_release
};

static struct miscdevice tsdev = {
	/*
	 * We don't care what minor number we end up with, so tell the
	 * kernel to just pick one.
	 */
	MISC_DYNAMIC_MINOR,
	/*
	 * Name ourselves /dev/tsdev.
	 */
	DEV_NAME,
	/*
	 * What functions to call when a program performs file
	 * operations on the device.
	 */
	&tuio_fops
};

/*
 * Initializes the device and creates the ring buffers.
 */
static int __init tuio_init(void)
{
	int ret;
   int i;
   struct buf_ring_ent *cur;

	/*
	 * Create the "tsdev" device in the /sys/class/misc directory.
	 * Udev will automatically create the /dev/tsdev device using
	 * the default rules.
	 */
	ret = misc_register(&tsdev);
	if (ret)
		printk(KERN_ERR
		       "Unable to register %s misc device\n", DEV_NAME);

   /* Setup the ring buffer */
   read_buf = write_buf = cur = kmalloc(sizeof(struct buf_ring_ent), GFP_USER);
   cur->len = 0;
   for (i = 1; i < BUF_COUNT; i++) {
      cur->next = kmalloc(sizeof(struct buf_ring_ent), GFP_USER);
      cur = cur->next;
      cur->len = 0;
   }
   /* close the loop */
   cur->next = read_buf;

	return ret;
}


/*
 * Deregisters the device and frees the ring buffer.
 */
static void __exit tuio_exit(void)
{
   struct buf_ring_ent *cur, *next = 0;

	misc_deregister(&tsdev);

   /* Free the ring buffer */
   cur = read_buf;
   while (next != read_buf) {
      next = cur->next;
      kfree(cur);
      cur = next;
   }
}


module_init(tuio_init);
module_exit(tuio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavan Trikutam <ptrikuta@calpoly.edu>");
MODULE_DESCRIPTION("TUIO message module");
MODULE_VERSION("0.5");
