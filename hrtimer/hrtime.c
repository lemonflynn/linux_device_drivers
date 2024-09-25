#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
 
#define TEST_CNT 20
struct hrtimer hrtimer_test_timer;
ktime_t m_kt;
static int test_cnt = TEST_CNT;
struct timespec time_record[TEST_CNT];
int record_pos;
char msg_buf[4096];
static int interval = 1000*10; //ns
module_param(interval, int, S_IRUGO);

static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;
 
static enum hrtimer_restart hrtimer_test_timer_poll(struct hrtimer *timer)
{
    struct timespec a;
    getnstimeofday(&a);
    time_record[record_pos++] = a;
	hrtimer_forward(timer, timer->base->get_time(), m_kt);//hrtimer_forward(timer, now, tick_period);
	//return HRTIMER_NORESTART;
    if(test_cnt--) {
	    return HRTIMER_RESTART;
    } else {
        flag = 1;
        wake_up_interruptible(&wq);
        return HRTIMER_NORESTART;
    }
}
 
ssize_t hrtimer_test_write(struct file *f, const char __user *buf, size_t t, loff_t *len)
{
    char k_buf;
    int msg_pos;
    int i;
   
    if(copy_from_user(&k_buf, buf, 1)) {
        return -EFAULT;
    }

	printk("hrtimer Debug buf:%x size:%d\n", k_buf, t);
    
	if(k_buf == '0'){
        printk("hrtimer_start\n");
        m_kt = ktime_set(0, interval); //sec, nsec
        test_cnt = TEST_CNT;
        memset(msg_buf, 0, 256);
        memset(time_record, 0, TEST_CNT * sizeof(struct timespec));
        record_pos = 0;
        hrtimer_start(&hrtimer_test_timer, m_kt, HRTIMER_MODE_REL);
        flag = 0;
        wait_event_interruptible(wq, flag != 0);

        msg_pos = 0;
        for(i = 1; i < record_pos; i++) {
            msg_pos += sprintf(&msg_buf[msg_pos],
                        "index %2i - interval %3lims_%3lius_%3lins\n",
                        i,
                        ((time_record[i].tv_nsec - time_record[i-1].tv_nsec)/1000000)%1000,
                        ((time_record[i].tv_nsec - time_record[i-1].tv_nsec)/1000)%1000,
                        (time_record[i].tv_nsec - time_record[i-1].tv_nsec)%1000);
        }
        printk("%s\n", msg_buf);
	} else if (k_buf == '1'){
		printk("hrtimer_cancel\n");
		hrtimer_cancel(&hrtimer_test_timer);
	}
 
	return t;
}

static const struct file_operations hrtimer_test_fops =
{
	.owner   	=   THIS_MODULE,
	.write		=   hrtimer_test_write,
	.unlocked_ioctl	= NULL,
};
 
struct miscdevice hrtimer_test_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hrtimer_test",
	.fops = &hrtimer_test_fops,
};
 
static int __init hrtimer_test_init(void)
{
	int ret;
 
    printk("hrtimer init, interval %dns\n", interval);
	ret = misc_register(&hrtimer_test_dev);
    hrtimer_init(&hrtimer_test_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_test_timer.function = hrtimer_test_timer_poll;
	return 0;
}
 
static void __exit hrtimer_test_exit(void)
{
    printk("hrtimer exit\n");
	misc_deregister(&hrtimer_test_dev);
}
 
module_init(hrtimer_test_init);  
module_exit(hrtimer_test_exit);
MODULE_AUTHOR("test");
MODULE_LICENSE("GPL");  
