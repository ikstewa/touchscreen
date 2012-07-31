/* Pavan Trikutam, Ian Stewart, Henry Phan
 * CPE454
 * Touch Screen Project
 *
 * Name: ts_dev.c
 * Description: This is the device that will accept and output TUIO 
 * information through its read/write functions. It also can process TUIO data
 * and output the same data in a different format.
 * Usage: Use as a device.
 *
 * Note: the /dev/hello_world tutorial on linuxdevcenter.com was used as a
 * starting point for this device file.
 * 
 * Modified:
 * 	05/08/2009	Pavan	Original Write
 */


#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#include <asm/uaccess.h>

#define BUF_LEN 100

//A few declarations that are useful
bool isEmpty = true;	//Any data in the device's buffer?
static char ts_str[BUF_LEN];

/*
 * As of now, this function will pass to the buffer the last character stream recieved.
 * If the stream is empty, then it passes "No data in buffer."
 */

static ssize_t ts_read(struct file * file, char * buf, 
			  size_t count, loff_t *ppos)
{
	char *emp_str = "No data in buffer.\n";
	int len;
	if (isEmpty)
	{
		len = strlen(emp_str); /* Don't include the null byte. */
		
		/*
		 * We only support reading the whole string at once.
		 */
		//if (count < len)
		//	return -EINVAL;
	}
	else
	{
		len = strlen(ts_str);
		if (count < len)
			return -EINVAL;
	}
	
	/*
	 * If file position is non-zero, then assume the string has
	 * been read and indicate there is no more data to be read.
	 */
	if (*ppos != 0)
		return 0;
	/*
	 * Besides copying the string to the user provided buffer,
	 * this function also checks that the user has permission to
	 * write to the buffer, that it is mapped, etc.
	 */
	if (isEmpty)
	{
		if (copy_to_user(buf,emp_str,len))
			return -EINVAL;
	}
	else
	{
		if (copy_to_user(buf, ts_str, len))
			return -EINVAL;
	}
	
	/*
	 * Tell the user how much data we wrote.
	 */
	*ppos = len;

	/* Reset the "empty variable to true. All data has been passed. */
	//Commented out so that the stored string is returned every time.
	//isEmpty = true;	
	return len;
}

static ssize_t ts_write(struct file * file, const char * buf, 
			size_t count, loff_t * offp)
{
	/* Data in the buffer is being overwritten. It is no longer empty. */
	isEmpty = false;

	/* Get the data and store it. */
	if (copy_from_user(ts_str,buf,count))
		return -EINVAL;

	return count;
}
/*
 * The only file operations we care about are read and write.
 */

static const struct file_operations ts_fops = {
	.owner		= THIS_MODULE,
	.read		= ts_read,
	.write		= ts_write,
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
	"tsdev",
	/*
	 * What functions to call when a program performs file
	 * operations on the device.
	 */
	&ts_fops
};

static int __init
ts_init(void)
{
	int ret;

	/*
	 * Create the "tsdev" device in the /sys/class/misc directory.
	 * Udev will automatically create the /dev/tsdev device using
	 * the default rules.
	 */
	ret = misc_register(&tsdev);
	if (ret)
		printk(KERN_ERR
		       "Unable to register \"Hello, world!\" misc device\n");

	return ret;
}

module_init(ts_init);

static void __exit
ts_exit(void)
{
	misc_deregister(&tsdev);
}

module_exit(ts_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pavan Trikutam <ptrikuta@calpoly.edu>");
MODULE_DESCRIPTION("Touch Screen TUIO module");
MODULE_VERSION("dev");
