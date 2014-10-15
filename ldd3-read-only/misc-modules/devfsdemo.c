#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>


MODULE_LICENSE("Dual BSD/GPL");

//Device vars
static struct class *my_class;
static dev_t my_devnum;
static struct cdev my_cdev;
static struct device *my_device;

//Parameters
#define BUFSIZE 32
static char buf[BUFSIZE] = "testi tapahtuma nyt";
static int rows = 4;
static int read_index = 0;
static int write_index = 0;
static int Device_Open = 0;

//Buffer operations
static int buf_get(char *buffer, const struct kernel_param *kp) {
    strim(buffer);
    return scnprintf(buffer, PAGE_SIZE, "%s", ((char *)kp->arg));
}
static int buf_set(const char *val, const struct kernel_param *kp) {  
  char *p = kp->arg;
  
  if(strlen(val) >= BUFSIZE) {
      printk(KERN_ERR "Given string is too long. Maximum length is %i\n", BUFSIZE - 1); //Final char is NUL
      return -EINVAL;
  }
  strcpy(p, val); //Store the value to the parameter
  strim(p); //New line is added at read, so we better trim it out
  
  printk(KERN_ALERT "Hello: New param %s\n", p);
  
  return 0;
}
static struct kernel_param_ops buf_ops = {
    .get = buf_get,
    .set = buf_set
}; //End buffer operations
module_param_cb(buffer, &buf_ops, buf, S_IRUGO | S_IWUSR);

//Row operations
int row_read(char *buffer, const struct kernel_param *kp) {
    return scnprintf(buffer, PAGE_SIZE, "%d", *((int *) kp->arg));
}
static int row_write(const char *val, const struct kernel_param *kp) {
    int value = *val - '0';
    
    switch (value) {
        case 1: case 2: case 4:
            rows = value;
            break;
        default:
            printk(KERN_ERR "Invalid value %i for rows. Acceptable values are: 1, 2 and 4\n", value);
            return value < 0 ? value : -EINVAL;
    }
    
    printk(KERN_ALERT "Value given is: %i\n", value);
    
    *((int *) kp->arg) = value;
    return 0;

}
static struct kernel_param_ops row_ops = {
    .get = row_read,
    .set = row_write
};
module_param_cb(rows, &row_ops, &rows, S_IRUGO | S_IWUSR);  //End row operations

//Device operations
static ssize_t buf_read (struct file *filp, char __user *ubuff, size_t len, loff_t *offs) {
    int charsPerRow = BUFSIZE / rows;
    int count = strlen(buf);
    char screen[BUFSIZE] = "";
    char c;
    int i;
    
    //We only support reading the whole string at once
    if(len < count) 
        return -EINVAL;
    
    /*If file position is non-zero, then assume the string has been
     * read and indicate there is no more data to read.
     */
    if(*offs != 0)
        return 0;

    /*
     * Besides copying the string to the user provided buffer,
     * this function also checks that the user has permission to
     * write to the buffer, that it is mapped, etc.
     */
//    for(i = 0; i < count; i++) {
//        if(strlen(screen) + 1 == BUFSIZE) break;
//        if(i % charsPerRow == 0 && i != 0) strcat(screen, "\n");
//        
//        c = buf[i];
//        strcat(screen, c);
//    }
//    
//    strcat(screen, "\0");
//    printk(KERN_ALERT "Copy to user\n");
    if (copy_to_user(ubuff, buf, count))
        return -EINVAL;
    
    /*
     * Tell the user how much data we wrote.
     */
    *offs = count;
    
    return count;
}
static ssize_t buf_write (struct file *filp, const char __user *ubuff, size_t len, loff_t *offs) {
    int remaining;
    int space_left;
    
    printk(KERN_ALERT "my_write\n");
    
    space_left = BUFSIZE - 1 - write_index + read_index;
    
    if(space_left > BUFSIZE) {
        space_left -= BUFSIZE;
    }
    
    if(len >= space_left) {
        len = space_left;
    }
    
    if(!access_ok(VERIFY_WRITE, ubuff, len)) {
        return -EFAULT;
    }
    
    remaining = copy_to_user(buf, ubuff, len);
    
    if(remaining) {
        return -EFAULT;
    }
    
    write_index += len;
    if(write_index >= BUFSIZE) {
        write_index -= BUFSIZE;
    }
    
    *offs += len;
    
    printk(KERN_ALERT "my_write got %d\n", len);
    
    return len;
}
//static int buf_open (struct inode *inode, struct file *filp) {
//    printk(KERN_ALERT "my_open\n");
//    return 0;
//}
//static int buf_release (struct inode *inode, struct file *filp) {
//    printk(KERN_ALERT "my_release\n");
//    return 0;
//}

static int device_open(struct inode *inode, struct file *file) {
#ifdef DEBUG
    printk(KERN_INFO "device_open(%p)\n", file);
#endif

    /* 
     * We don't want to talk to two processes at the same time 
     */
    if (Device_Open)
        return -EBUSY;

    Device_Open++;
    /*
     * Initialize the message 
     */
//    Message_Ptr = Message;
    try_module_get(THIS_MODULE);
    return 0;
}
static int device_release(struct inode *inode, struct file *file) {
#ifdef DEBUG
    printk(KERN_INFO "device_release(%p,%p)\n", inode, file);
#endif

    /* 
     * We're now ready for our next caller 
     */
    Device_Open--;

    module_put(THIS_MODULE);
    return 0;
}
static struct file_operations fileops = {
    .read = buf_read,
    .write = buf_write,
    .open = device_open,
    .release = device_release
            //single_release seq_release swaps_open scull_open
}; //End device operations

static int dev_init(void) {
    int err;

    printk(KERN_ALERT "devfsdemo started\n");

    //1: create class
    my_class = class_create(THIS_MODULE, "devfsdemo");

    //2: alloc chardev nums
    err = alloc_chrdev_region(&my_devnum, 0, 1, "devfsdemo"); //Allocate/reserve a device number for devdemo

    if (err) {
        printk(KERN_ERR "Error in reserving devnum %d\n", err);
    }

    printk(KERN_ALERT "Device number reserved %d:%d\n",
            MAJOR(my_devnum), MINOR(my_devnum)); //Print to make sure the device has allocated a region

    //3: init cdev
    cdev_init(&my_cdev, &fileops);
    my_cdev.owner = THIS_MODULE;
    
    //4: add cdev
    cdev_add(&my_cdev, my_devnum, 1);

    //5: create device
    my_device = device_create(my_class, NULL, my_devnum, NULL, "devfsdemo");

    return err;
}

static void dev_exit(void) {
    printk(KERN_ALERT "Terminating devfsdemo\n");

    device_destroy(my_class, my_devnum);
    cdev_del(&my_cdev);
    unregister_chrdev_region(my_devnum, 1);
    class_destroy(my_class); //We will destroy our class from /sys/class
}

module_init(dev_init);
module_exit(dev_exit);