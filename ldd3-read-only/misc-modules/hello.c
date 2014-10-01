/*                                                     
 * $Id: hello.c,v 1.5 2004/10/26 03:32:21 corbet Exp $ 
 */                                                    
#include <linux/init.h>
#include <linux/module.h>
MODULE_LICENSE("Dual BSD/GPL");

//Module parameters
static int my_parameter = 0;
module_param(my_parameter, int, S_IRUGO | S_IWUSR);

static int hello_init(void)
{
	printk(KERN_ALERT "Hello, world\n");
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_ALERT "Goodbye, cruel world. %d\n", my_parameter);
}

module_init(hello_init);
module_exit(hello_exit);
