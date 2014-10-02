/*                                                     
 * $Id: hello.c,v 1.5 2004/10/26 03:32:21 corbet Exp $ 
 */                                                    
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
//#include <fcntl.h>
//#include <sys/stat.h>
MODULE_LICENSE("Dual BSD/GPL");

//Module parameters
static int x, y, tulo, lukujen_maara = 0;

static int tulo_get(char *buffer, const struct kernel_param *kp) {
    return scnprintf(buffer, PAGE_SIZE, "%d", *((int *)kp->arg));
}

static int tulo_set(const char *val, const struct kernel_param *kp) {
  long l;
  int ret;
  
  ret = kstrtoul(val, 0, &l);
  if (ret < 0 || ((int)l != l))	{
      printk(KERN_ERR "Failed\n");
      return ret < 0 ? ret : -EINVAL;
  }
    
  *((int *)kp->arg) = l;

  printk(KERN_ALERT "Hello: New param %ld\n", l);

  return 0;
}

static struct kernel_param_ops tulo_ops = {
    .set = tulo_set,
    .get = tulo_get
};

module_param_cb(x, &tulo_ops, &tulo, S_IRUGO | S_IWUSR);
module_param_cb(y, &tulo_ops, &tulo, S_IRUGO | S_IWUSR);
module_param(lukujen_maara, int, S_IRUGO);
module_param_cb(tulo, &tulo_ops, &tulo, S_IRUGO | S_IWUSR);

static int hello_init(void)
{
	printk(KERN_ALERT "Hello, world! Tulo: %d\n", tulo_ops);
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(hello_init);
module_exit(hello_exit);
