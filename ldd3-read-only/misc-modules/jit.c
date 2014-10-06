/*
 * jit.c -- the just-in-time module
 *
 * Copyright (C) 2001,2003 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001,2003 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: jit.c,v 1.16 2004/09/26 07:02:43 gregkh Exp $
 */

//#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/hardirq.h>
/*
 * This module is a silly one: it only embeds short code fragments
 * that show how time delays can be handled in the kernel.
 */

int delay = HZ; /* the default delay, expressed in jiffies */

module_param(delay, int, 0);

MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("Dual BSD/GPL");

/* use these as data pointers, to implement four files in one function */
enum jit_files {
    JIT_BUSY,
    JIT_SCHED,
    JIT_QUEUE,
    JIT_SCHEDTO
};

int tdelay = 10;
module_param(tdelay, int, 0);

/* This data structure used as "data" for the timer and tasklet functions */
struct jit_data {
    struct timer_list timer;
    struct tasklet_struct tlet;
    int hi; /* tasklet or tasklet_hi */
    wait_queue_head_t wait;
    unsigned long prevjiffies;
    unsigned char *buf;
    int loops;
};
#define JIT_ASYNC_LOOPS 5

/*
 * The file operations structure contains our open function along with
 * set of the canned seq_ ops.
 */
static ssize_t jit_fn(struct file *filp, char __user *ubuff, size_t len, loff_t *offs) {
    unsigned long j0, j1; /* jiffies */
    wait_queue_head_t wait;

    init_waitqueue_head(&wait);
    j0 = jiffies;
    j1 = j0 + delay;

    switch ((long) offs) {
        case JIT_BUSY:
            while (time_before(jiffies, j1))
                cpu_relax();
            break;
        case JIT_SCHED:
            while (time_before(jiffies, j1)) {
                schedule();
            }
            break;
        case JIT_QUEUE:
            wait_event_interruptible_timeout(wait, 0, delay);
            break;
        case JIT_SCHEDTO:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(delay);
            break;
    }
    j1 = jiffies; /* actual value after we delayed */

    //    len = sprintf(buf, "%9li %9li\n", j0, j1);
    //    *start = buf;
    return len;
}

static int jit_currenttime(struct file *filp, char __user *ubuff, size_t len, loff_t *offs) {
    struct timeval tv1;
    struct timespec tv2;
    unsigned long j1;
    u64 j2;

    /* get them four */
    j1 = jiffies;
    j2 = get_jiffies_64();
    do_gettimeofday(&tv1);
    tv2 = current_kernel_time();

    /* print */
    len = 0;
    len += sprintf(buf, "0x%08lx 0x%016Lx %10i.%06i\n"
            "%40i.%09i\n",
            j1, j2,
            (int) tv1.tv_sec, (int) tv1.tv_usec,
            (int) tv2.tv_sec, (int) tv2.tv_nsec);
    *start = buf;
    return len;
}

static struct file_operations fops = {
    .read = jit_fn
};

static struct file_operations time_fops = {
    .read = jit_currenttime
};

int __init jit_init(void) {
    proc_create_data("currenttime", 0, NULL, &time_fops, NULL);
    proc_create_data("jitbusy", 0, NULL, &fops, (void *) JIT_BUSY);
    proc_create_data("jitsched", 0, NULL, &fops, (void *) JIT_SCHED);
    proc_create_data("jitqueue", 0, NULL, &fops, (void *) JIT_QUEUE);
    proc_create_data("jitschedto", 0, NULL, &fops, (void *) JIT_SCHEDTO);

    proc_create_data("jitimer", 0, NULL, &fops, NULL);
    proc_create_data("jitasklet", 0, NULL, &fops, NULL);
    proc_create_data("jitasklethi", 0, NULL, &fops, (void *) 1);

    return 0; /* success */
}

void __exit jit_cleanup(void) {
    remove_proc_entry("currentime", NULL);
    remove_proc_entry("jitbusy", NULL);
    remove_proc_entry("jitsched", NULL);
    remove_proc_entry("jitqueue", NULL);
    remove_proc_entry("jitschedto", NULL);

    remove_proc_entry("jitimer", NULL);
    remove_proc_entry("jitasklet", NULL);
    remove_proc_entry("jitasklethi", NULL);
}

module_init(jit_init);
module_exit(jit_cleanup);
