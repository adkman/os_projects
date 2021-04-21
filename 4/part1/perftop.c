#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int perftop_proc_show(struct seq_file *s, void *v)
{
    seq_printf(s, "Hello World\n");
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

static int __init on_init(void)
{
    proc_create("perftop", 0, NULL, &perftop_proc_ops);
    return 0;
}

static void __exit on_exit(void)
{
    remove_proc_entry("perftop", NULL);
}

module_init(on_init);
module_exit(on_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish Adkar <madkar@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Project 4 part 1");
