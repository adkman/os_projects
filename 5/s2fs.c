#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>

#define LFS_MAGIC 0x16980122

static int s2fs_open (struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t s2fs_read_file (struct file *filp, char *buf,
        size_t count, loff_t *offset)
{
    char *msg  = "Hello World!\n";
    int len = strlen(msg);

    if (*offset > len)
    {
        return 0;
    }

    if (count > len - *offset)
    {
        count = len - *offset;
    }

    if (copy_to_user(buf, msg + *offset, count))
    {
        return -EFAULT;
    }
    *offset += count;
    return count;
}

static ssize_t s2fs_write_file (struct file *filp, const char *buf,
        size_t count, loff_t *offset)
{
    return 0;
}

static struct file_operations s2fs_file_ops = {
    .open   = s2fs_open,
    .read   = s2fs_read_file,
    .write  = s2fs_write_file,
};

static struct inode *s2fs_make_inode (struct  super_block *sb, int mode)
{
    struct inode *inode = new_inode(sb);

    if (inode)
    {
        inode->i_ino = get_next_ino();
        inode->i_mode = mode;
        inode->i_uid = KUIDT_INIT(0);
        inode->i_gid = KGIDT_INIT(0);
        inode->i_size = VMACACHE_SIZE;
        inode->i_blocks = 0;
        inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    }
    return inode;
}

static struct dentry *s2fs_create_file (struct super_block *sb,
        struct dentry *dir, const char *file_name)
{
    struct dentry *dentry;
    struct inode *inode;

    dentry = d_alloc_name(dir, file_name);
    if (! dentry)
    {
        return 0;
    }

    inode = s2fs_make_inode(sb, S_IFREG | 0644);
    if (! inode)
    {
        dput(dentry);
        return 0;
    }
    inode->i_fop = &s2fs_file_ops;

    d_add(dentry, inode);

    return dentry;
}

static struct dentry *s2fs_create_dir (struct super_block *sb,
        struct dentry *parent, const char *dir_name)
{
    struct dentry *dentry;
    struct inode *inode;

    dentry = d_alloc_name(parent, dir_name);
    if (! dentry)
    {
        return 0;
    }

    inode = s2fs_make_inode(sb, S_IFDIR | 0755);
    if (! inode)
    {
        dput(dentry);
        return 0;
    }
    inode->i_op = &simple_dir_inode_operations;
    inode->i_fop = &simple_dir_operations;

    d_add(dentry, inode);

    return dentry;
}

static struct super_operations s2fs_s_ops = {
    .statfs     = simple_statfs,
    .drop_inode = generic_delete_inode,
};

static int s2fs_fill_super(struct super_block *sb,
        void *data, int silent)
{
    struct dentry *root_dentry, *subdir_dentry;
    struct inode *root_inode;

    sb->s_blocksize = VMACACHE_SIZE;
    sb->s_blocksize_bits = VMACACHE_SIZE;
    sb->s_magic = LFS_MAGIC;
    sb->s_op = &s2fs_s_ops;

    root_inode = s2fs_make_inode(sb, S_IFDIR | 0755);
    if (! root_inode)
    {
        return -ENOMEM;
    }
    root_inode->i_op = &simple_dir_inode_operations;
    root_inode->i_fop = &simple_dir_operations;
    set_nlink(root_inode, 2);

    root_dentry = d_make_root(root_inode);
    if (! root_dentry)
    {
        iput(root_inode);
        return -ENOMEM;
    }
    sb->s_root = root_dentry;

    subdir_dentry = s2fs_create_dir(sb, root_dentry, "foo");
    if (subdir_dentry)
    {
        s2fs_create_file(sb, subdir_dentry, "bar");
    }
    return 0;
}

static struct dentry *s2fs_get_super(struct file_system_type *fst,
        int flags, const char *devname, void *data)
{
    return mount_nodev(fst, flags, data, s2fs_fill_super);
}

static struct file_system_type s2fs_type = {
    .owner      = THIS_MODULE,
    .name       = "s2fs",
    .mount      = s2fs_get_super,
    .kill_sb    = kill_litter_super,
};

static int __init on_init(void)
{
    return register_filesystem(&s2fs_type);
}

static void __exit on_exit(void)
{
    unregister_filesystem(&s2fs_type);
}

module_init(on_init);
module_exit(on_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish Adkar <madkar@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Project 5");
