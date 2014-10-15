/* Buttons and LEDs demo
 * (c) Lauri Pirttiaho, 2014
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/spi/mcp23s08.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

/************************************************************
 * Module config
 */

#define MODULE_NAME "buttons"

/************************************************************
 * Module data
 */

static int gpiobase = 240;

module_param(gpiobase, int, 0444);

/************************************************************
 * Button control
 */

// Buttons (5 buttons of the Adafruit 1110)
#define SELECT 0
#define RIGHT  1
#define DOWN   2
#define UP     3
#define LEFT   4

// LEDs (3 back-light leds of the Adafruit 1110)
#define RED   6
#define GREEN 7
#define BLUE  8

#define IN(pin) gpio_request_one(gpiobase+(pin), GPIOF_IN, #pin)
#define DIN(pin) gpio_direction_input(gpiobase+(pin))
#define OUT(pin) gpio_request_one(gpiobase+(pin), GPIOF_OUT_INIT_HIGH, #pin)
#define DOUT(pin) gpio_direction_output(gpiobase+(pin))
#define FREE(pin) gpio_free(gpiobase+(pin))

#define SET(pin, value) gpio_set_value_cansleep(gpiobase+(pin), (value))
#define GET(pin) gpio_get_value_cansleep(gpiobase+(pin))

static struct i2c_adapter* bus1;
static struct i2c_client* cli32;

static void io_init(void)
{
  struct mcp23s08_platform_data mcp23017_pfdata = {
    .chip = {
      [0] = {
	.is_present = 1,
	.pullups = 0x001F /* pins [4:0] */
      }
    },
    .base = gpiobase
  };

  struct i2c_board_info mcp23017_info = {
    .type = "mcp23017",
    .addr = 32,
    .platform_data = &mcp23017_pfdata
  };

  bus1 = i2c_get_adapter(1);
  printk(KERN_ALERT "***bus1 = %p\n", bus1);  
  cli32 = i2c_new_device(bus1, &mcp23017_info); 
  printk(KERN_ALERT "***cli32 = %p\n", cli32);  

  IN(SELECT);
  IN(RIGHT);
  IN(DOWN);
  IN(UP);
  IN(LEFT);
  
  OUT(RED);
  OUT(GREEN);
  OUT(BLUE);
}

static void io_exit(void)
{
  DIN(RED);
  DIN(GREEN);
  DIN(BLUE);

  FREE(SELECT);
  FREE(RIGHT);
  FREE(DOWN);
  FREE(UP);
  FREE(LEFT);
  
  FREE(RED);
  FREE(GREEN);
  FREE(BLUE);

  if (cli32) {
    printk(KERN_ALERT "---cli32 = %p\n", cli32);  
    i2c_unregister_device(cli32);
  }
  if (bus1) {
    printk(KERN_ALERT "---bus1 = %p\n", bus1);  
    i2c_put_adapter(bus1);
  }
}

/************************************************************
 * Light state machine
 */

static bool red_on = 0;
static bool green_on = 0;

module_param_named(R, red_on, bool, 0444);
module_param_named(G, green_on, bool, 0444);

static void red_set(int n)
{
  red_on = n;
  SET(RED, 1-n);
}

static void green_set(int n)
{
  green_on = n;
  SET(GREEN, 1-n);
}

static void event(int n)
{
  switch (n) {
  case 1:
    if (! red_on && ! green_on) {
      red_set(1);
    } else if (red_on) {
      red_set(0);
    } else if (green_on) {
      red_set(1);
      green_set(0);
    }
    break;
  case 2:
    if (! red_on && ! green_on) {
      green_set(1);
    } else if (green_on) {
      green_set(0);
    } else if (red_on) {
      green_set(1);
      red_set(0);
    }
    break;
  }
}

/************************************************************
 * Button scanner
 */

static int buttons_before;

static int button_events;
static spinlock_t button_events_sl;
static wait_queue_head_t readq;

module_param(button_events, int, 0644);

static void scan_buttons(void)
{
  int buttons_now = 0;

  for (int i = 0; i < 5; ++i) {
    buttons_now |= GET(i) << i;
  }

  int new_events = (buttons_before & ~buttons_now) & 0x1F;

  if (new_events & (1<<DOWN)) {
    event(1);
  }

  if (new_events & (1<<UP)) {
    event(2);
  }

  spin_lock(&button_events_sl);
  button_events |= new_events;
  spin_unlock(&button_events_sl);

  if (new_events & ((1<<DOWN)|(1<<UP))) {
    wake_up_interruptible(&readq);
  }

  buttons_before = buttons_now;
}

// Scanning work queue. Timer won't work since GPIO calls
// may block which is not allowed in timer's interrupt
// context!

static struct workqueue_struct* scanner_q;
static struct delayed_work scanner_w;

// Scanning frequency in Hz
#define SCAN_FRQ 50

static void scanner_work(struct work_struct *work)
{
  scan_buttons();
  PREPARE_DELAYED_WORK(&scanner_w, scanner_work);
  queue_delayed_work(scanner_q, &scanner_w, HZ/SCAN_FRQ);
}

static void scanner_init(void)
{
  buttons_before = 0x1F;
  button_events = 0;
  spin_lock_init(&button_events_sl);

  scanner_q = alloc_workqueue(MODULE_NAME "_q", WQ_UNBOUND, 1);
  INIT_DELAYED_WORK(&scanner_w, scanner_work);
  queue_delayed_work(scanner_q, &scanner_w, HZ/SCAN_FRQ);
}

static void scanner_exit(void)
{
  cancel_delayed_work_sync(&scanner_w);
  flush_workqueue(scanner_q);
  destroy_workqueue(scanner_q);
}

/************************************************************
 * Fileops
 */

bool event_read;

static int dev_open(struct inode *inode, struct file *filp)
{
  event_read = 0;
  spin_lock(&button_events_sl);
  button_events = 0;
  spin_unlock(&button_events_sl);
  return 0;
}

static ssize_t dev_read(
    struct file *filp, char __user *ubuff, size_t len, loff_t *offs)
{
  int ret = 0;
  char buffer[32];

  if (event_read) {
    event_read = 0;
    return 0;
  }

  ret = wait_event_interruptible_exclusive(readq, button_events & ((1<<UP)|(1<<DOWN)));
  
  // Wake up from interrupts, try again
  if (ret) return -ERESTARTSYS;

  event_read = 1;
  len = 1;

  spin_lock(&button_events_sl);
  if (button_events & (1<<DOWN)) {
    buffer[0] = '1';
    button_events &= ~(1<<DOWN);
  } else if (button_events & (1<<UP)) {
    buffer[0] = '2';
    button_events &= ~(1<<UP);
  }
  spin_unlock(&button_events_sl);

  int uncopied = copy_to_user(ubuff, buffer, len);

  if (uncopied) {
    ret = -EFAULT;
  } else {
    ret = len;
  }

  return ret;
}

static ssize_t dev_write(struct file *filp, const char __user *ubuff, size_t len, loff_t *offs)
{
  int ret = 0;
  char buffer[32];
  if (len > 32) {
    len = 32;
  }
  
  int uncopied = copy_from_user(buffer, ubuff, len);

  if (uncopied) {
    ret = -EFAULT;
  } else {
    ret = len;
  }

  for (int i = 0; i < len; ++i) {
    if (buffer[i] == '1' || buffer[i] == '2') {
      event(buffer[i]-'0');
    }
  }

  return ret;
}

static int dev_release(struct inode *inode, struct file *filp)
{
  return 0;
}

static struct file_operations fileops = {
  .owner = THIS_MODULE,
  .open = dev_open,
  .read = dev_read,
  .write = dev_write,
  .release = dev_release
};


/************************************************************
 * Device data
 */

static struct class *class;
static dev_t devnum;
struct cdev cdev;
struct device *dev;

/************************************************************
 * Init and exit
 */

static int buttons_init(void)
{
  int err = 0;

  // Create device class
  class = class_create(THIS_MODULE, MODULE_NAME);

  // Allocate device number
  err = alloc_chrdev_region(&devnum, 0, 1, MODULE_NAME);
  if (err) {
    goto devnum_fail;
  }

  // Create cdev
  cdev_init(&cdev, &fileops);
  err = cdev_add(&cdev, devnum, 1);
  if (err) {
    goto dev_add_fail;
  }

  // Create device to /dev
  dev = device_create(class, NULL, devnum, NULL, MODULE_NAME);
  if (IS_ERR(dev)) {
    err = PTR_ERR(dev);
    goto dev_create_fail;
  }

  // All OK
  init_waitqueue_head(&readq);
  io_init();
  scanner_init();

  return 0;

 dev_create_fail:
  cdev_del(&cdev);

 dev_add_fail:
  unregister_chrdev_region(devnum, 1);  

 devnum_fail:
  class_destroy(class);

  return err;
}

static void buttons_exit(void)
{
  printk(KERN_ALERT "---exit\n");  
  
  scanner_exit();
  io_exit();

  device_destroy(class, devnum);
  cdev_del(&cdev);
  unregister_chrdev_region(devnum, 1);  
  class_destroy(class);
}

module_init(buttons_init);
module_exit(buttons_exit);

MODULE_AUTHOR("Lauri Pirttiaho <lapi@cw.fi>");
MODULE_LICENSE("Dual BSD/GPL");
