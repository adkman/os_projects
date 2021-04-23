#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kprobes.h>
#include <linux/spinlock.h>

static spinlock_t pre_count_lock;
static spinlock_t post_count_lock;

unsigned long int pre_count = 0;
unsigned long int post_count = 0;

static int perftop_proc_show(struct seq_file *s, void *v)
{
    seq_printf(s, "Hello World\n");

    spin_lock(&pre_count_lock);
    seq_printf(s, "pre_count = %lu\n", pre_count);
    spin_unlock(&pre_count_lock);

    spin_lock(&post_count_lock);
    seq_printf(s, "post_count = %lu\n", post_count);
    spin_unlock(&post_count_lock);

    return 0;
}

static int perftop_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, perftop_proc_show, NULL);
}

static const struct proc_ops perftop_proc_ops = {
    .proc_open = perftop_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int entry_pick_next_fair(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    spin_lock(&pre_count_lock);
    pre_count++;
    spin_unlock(&pre_count_lock);
    return 0;
}
NOKPROBE_SYMBOL(entry_pick_next_fair);

static int ret_pick_next_fair(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    spin_lock(&post_count_lock);
    post_count++;
    spin_unlock(&post_count_lock);
    return 0;
}
NOKPROBE_SYMBOL(ret_pick_next_fair);

static struct kretprobe my_kretprobe = {
    .handler = ret_pick_next_fair,
    .entry_handler = entry_pick_next_fair,
    .maxactive = 1000
};

static int __init on_init(void)
{
    int ret;

    spin_lock_init(&pre_count_lock);
    spin_lock_init(&post_count_lock);

    proc_create("perftop", 0, NULL, &perftop_proc_ops);
    my_kretprobe.kp.symbol_name = "pick_next_task_fair";

    if ((ret = register_kretprobe(&my_kretprobe)) < 0)
    {
        printk("register_kretprobe failed, returned %d\n", ret);
        return -1;
    }
    printk("Registered kretprobe at %p\n", my_kretprobe.kp.addr);

    return 0;
}

static void __exit on_exit(void)
{
    unregister_kretprobe(&my_kretprobe);
    printk("Missed probing %d instances of %s\n", my_kretprobe.nmissed, my_kretprobe.kp.symbol_name);
    remove_proc_entry("perftop", NULL);
}

module_init(on_init);
module_exit(on_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish Adkar <madkar@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Project 4 part 1");
