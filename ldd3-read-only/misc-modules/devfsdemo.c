#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>


MODULE_LICENSE("Dual BSD/GPL");

//Device vars
static struct class *my_class;
static dev_t my_devnum;
static struct cdev my_cdev;
static struct device *my_device;

//Parameters
#define BUFSIZE 80
static char buf[BUFSIZE] = "testi";
static int rows = 1;

//Buffer operations
static int buf_get(char *buffer, const struct kernel_param *kp) {
    return scnprintf(buffer, PAGE_SIZE, "%s", ((char *)kp->arg));
}

static int buf_set(const char *val, const struct kernel_param *kp) {  
  char *p = kp->arg;
  strim(val);
  if(strlen(val) >= BUFSIZE) {
      printk(KERN_ERR "Given string is too long. Maximum length is %i\n", BUFSIZE - 1); //Final char is NUL
      return -EINVAL;
  }
  strcpy(p, val); //Store the value to the parameter
  strim(p); //New line is added at read, so we better trim it out
  
  printk(KERN_ALERT "Hello: New param %s\n", p);
  
  return 0;
}

static struct kernel_param_ops dev_ops = {
    .get = buf_get,
    .set = buf_set
}; //End buffer operations

//module_param(buf, charp, S_IRUGO | S_IWUSR);
module_param_cb(buffer, &dev_ops, buf, S_IRUGO | S_IWUSR);

//Row operations
int row_read(char *buffer, const struct kernel_param *kp) {
    return scnprintf(buffer, PAGE_SIZE, "%d", *((int *) kp->arg));
}
static int row_write(const char *val, const struct kernel_param *kp) {
    int value = *val - '0';
    
//    int value;
//    value = kstrtoul(val, 0, &l);
    
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
    cdev_init(&my_cdev, &dev_ops);
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