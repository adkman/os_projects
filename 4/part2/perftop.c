#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kprobes.h>
#include <linux/spinlock.h>
#include <linux/hashtable.h>
#include <linux/jhash.h>
#include <linux/slab.h>

static spinlock_t pre_count_lock;
static spinlock_t post_count_lock;
static spinlock_t context_switch_count_lock;

struct prev_task_data {
    unsigned long prev;
};

struct my_hashmap {
    DECLARE_HASHTABLE(my_hasht, 14);
};
struct my_hashmap *my_hashm; 
struct hashmap_entity {
    pid_t pid;
    unsigned long long start_tsc;
    struct hlist_node node;
};

struct rb_root sched_tasks_rbtree = RB_ROOT;
struct task_data {
    pid_t pid;
    unsigned long long total_tsc;
    struct rb_node node;
};

/* Hash Table methods - START */
static void add_to_hashm(pid_t pid, unsigned long long start_tsc, struct my_hashmap *hashm)
{
    int key = jhash(&pid, sizeof(pid_t), 0);
    struct hashmap_entity *entity;

    entity = kmalloc(sizeof(struct hashmap_entity), GFP_KERNEL);
    entity->pid = pid;
    entity->start_tsc = start_tsc;

    hash_add(hashm->my_hasht, &entity->node, key);
}

static struct hashmap_entity * get_entity_from_hashmap(pid_t pid, struct my_hashmap *hashm)
{
    int key = jhash(&pid, sizeof(pid_t), 0);
    struct hashmap_entity *entity;

    hash_for_each_possible(hashm->my_hasht, entity, node, key) {
        if (entity->pid == pid) {
            return entity;
        }
    }
    return NULL;
}

static void remove_from_hashm(struct my_hashmap *hashm)
{
    unsigned int bkt;
    struct hashmap_entity *curr;

    hash_for_each(hashm->my_hasht, bkt, curr, node)
    {
        hash_del(&curr->node);
        kfree(curr);
    }
}

static void destruct_hashm(struct my_hashmap *hashm)
{
    if (!hash_empty(hashm->my_hasht))
    {
        remove_from_hashm(hashm);
    }
    kfree(hashm);
}
/* Hash Table methods - END */

/* Red-Black Tree methods - START */
static void insert_rbtree(struct task_data *entity, struct rb_root *tree_root)
{
    struct rb_node **curr = &tree_root->rb_node;
    struct rb_node *parent = NULL;
    struct task_data *temp;

    while (*curr)
    {
        parent = *curr;
        temp = rb_entry(parent, struct task_data, node);
        if (entity->total_tsc < temp->total_tsc)
        {
            curr = &parent->rb_left;
        }
        else
        {
            curr = &parent->rb_right;
        }
    }
    rb_link_node(&entity->node, parent, curr);
    rb_insert_color(&entity->node, tree_root);
}

static struct task_data * lookup_and_remove(pid_t pid, struct rb_root *tree_root)
{
    struct rb_node *tmp_node;
    struct task_data *data;

    data = NULL;
    tmp_node = rb_first(tree_root);
    while (tmp_node)
    {
        data = rb_entry(tmp_node, struct task_data, node);
        if (data->pid == pid)
        {
            rb_erase(tmp_node, tree_root);
            break;
        }
        tmp_node = rb_next(tmp_node);
    }
    return data;
}

static void printTopTen(struct rb_root *tree_root)
{
    int t = 10;
    struct rb_node *curr;
    struct task_data *data;

    curr = rb_first(tree_root);
    while (t-- && curr)
    {
        data = rb_entry(curr, struct task_data, node);
        printk("PID: %d, Total tsc: %llu\n", data->pid, data->total_tsc);
        curr = rb_next(curr);
    }
}

static void destruct_rbtree(struct rb_root *tree_root)
{
    struct rb_node *tmp_node, *next_node;
    struct task_data *tmp_entity;

    tmp_node = rb_first(tree_root);
    while(tmp_node)
    {
        next_node = rb_next(tmp_node);
        tmp_entity = rb_entry(tmp_node, struct task_data, node);
        rb_erase(tmp_node, tree_root);
        kfree(tmp_entity);
        tmp_node = next_node;
    }
}
/* Red-Black Tree methods - END */

unsigned long int pre_count = 0;
unsigned long int post_count = 0;
unsigned long int context_switch_count = 0;

static int perftop_proc_show(struct seq_file *s, void *v)
{
    seq_printf(s, "Hello World\n");

    spin_lock(&pre_count_lock);
    seq_printf(s, "pre_count = %lu\n", pre_count);
    spin_unlock(&pre_count_lock);

    spin_lock(&post_count_lock);
    seq_printf(s, "post_count = %lu\n", post_count);
    spin_unlock(&post_count_lock);

    spin_lock(&context_switch_count_lock);
    seq_printf(s, "context_switch_count = %lu\n", context_switch_count);
    spin_unlock(&context_switch_count_lock);

    printTopTen(&sched_tasks_rbtree);

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
    struct prev_task_data *prev_data;
    struct task_data *data;
    struct task_struct *prev;
    struct hashmap_entity *hashed_entity;
    unsigned long long elapsed_tsc;

    spin_lock(&pre_count_lock);
    pre_count++;
    spin_unlock(&pre_count_lock);

    prev_data = (struct prev_task_data *)ri->data;
    prev_data->prev = regs->si; // Second argument of pick_next_task_fair

    prev = (struct task_struct *)regs->si;

    hashed_entity = get_entity_from_hashmap(prev->pid, my_hashm);
    if (hashed_entity != NULL)
    {
        elapsed_tsc = rdtsc() - hashed_entity->start_tsc;

        data = lookup_and_remove(prev->pid, &sched_tasks_rbtree);
        if (data != NULL)
        {
            data->total_tsc += elapsed_tsc;
            insert_rbtree(data, &sched_tasks_rbtree);
        }
    }

    return 0;
}
NOKPROBE_SYMBOL(entry_pick_next_fair);

static int ret_pick_next_fair(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct prev_task_data *prev_data;
    struct task_struct *next;
    unsigned long long current_tsc;

    spin_lock(&post_count_lock);
    post_count++;
    spin_unlock(&post_count_lock);

    prev_data = (struct prev_task_data *)ri->data;

    next = (struct task_struct *)regs->ax;
//    printk("%lu\t->\t%lu\n", data->prev, regs->ax);

    if (prev_data->prev != regs->ax) // regs->ax will contain the return value of pick_next_task_fair
    {
        spin_lock(&context_switch_count_lock);
        context_switch_count++;
        spin_unlock(&context_switch_count_lock);
    }

    current_tsc = rdtsc();
    add_to_hashm(next->pid, current_tsc, my_hashm);

    return 0;
}
NOKPROBE_SYMBOL(ret_pick_next_fair);

static struct kretprobe my_kretprobe = {
    .handler = ret_pick_next_fair,
    .entry_handler = entry_pick_next_fair,
    .maxactive = 9999
};

static int __init on_init(void)
{
    int ret;

    spin_lock_init(&pre_count_lock);
    spin_lock_init(&post_count_lock);
    spin_lock_init(&context_switch_count_lock);

    my_hashm = kmalloc(sizeof(struct my_hashmap), GFP_KERNEL);
    hash_init(my_hashm->my_hasht);

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

    destruct_hashm(my_hashm);

    destruct_rbtree(&sched_tasks_rbtree);

    remove_proc_entry("perftop", NULL);
}

module_init(on_init);
module_exit(on_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish Adkar <madkar@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Project 4 part 1");
