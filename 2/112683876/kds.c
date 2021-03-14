#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h> 
#include <linux/hashtable.h>

#define DECIMAL_BASE 10
#define RDX_ODD_MARK 1
#define MAX_RDX_SIZE 1000
#define MAX_BMAP_ENTRY_NUM 1001

static char *int_str = "11 44 22 33 5";
static int *input;
static int n_len;

/* Linked List members */
LIST_HEAD(my_llist);			/* Linked List (1) */
struct num_l_entity
{
	int data;
	struct list_head list;
};

/* Red-Black Tree members */
struct rb_root my_rbtree = RB_ROOT;	/* Red-Black Tree (1) */
struct num_rb_entity
{
	int data;
	struct rb_node node;
};

/* Hash Table members */
struct my_hashmap {			/* Hash Table (1) */
	DECLARE_HASHTABLE(my_hasht, 14);
};
struct my_hashmap *my_hashm;		/* Hash Table (1) */
struct num_h_entity
{
	int data;
	struct hlist_node node;
};

/* Radix Tree members */
RADIX_TREE(my_rdxtree, GFP_KERNEL);	/* Radix Tree (1) */
struct num_rdx_entity
{
	int data;
};

/* XArray members */
DEFINE_XARRAY(my_xarray);		/* XArray (1) */

/* Bitmap members */
DECLARE_BITMAP(my_bmap, MAX_BMAP_ENTRY_NUM);	/* Bitmap (1) */


module_param(int_str, charp, 0);
MODULE_PARM_DESC(int_str, "A string of integers. Like '11 44 22 33 5'");

/* Linked List methods - START */
static void define_llist(int *data, int length, struct list_head *head)
{
	int i;

	for (i = 0; i < length; i++)
	{
		struct num_l_entity *num = kmalloc(sizeof(struct num_l_entity), GFP_KERNEL);
		num->data = data[i];
		INIT_LIST_HEAD(&num->list);

		list_add(&num->list, head);	/* Adding the item after the list head */
	}					/* So essentially, the order of the items */
}						/* will be reversed! */

static void iterate_llist(struct list_head *head)
{
	struct num_l_entity *curr_num;

	printk(KERN_INFO "Iterating over the Linked List...\n");
	list_for_each_entry(curr_num, head, list)
	{
		printk(KERN_INFO "Linux Linked List: Entity: %d\n", curr_num->data);
	}
}

static void destruct_llist(struct list_head *head)
{
	struct num_l_entity *curr_num, *next;

	printk(KERN_INFO "Deleting entities from Linux Linked List!\n");
	list_for_each_entry_safe(curr_num, next, head, list)
	{
		list_del(&curr_num->list);
		kfree(curr_num);
	}
}
/* Linked List methods - END */

/* Red-Black Tree methods - START */
static void insert(struct num_rb_entity *entity, struct rb_root *tree_root)
{
	struct rb_node **curr = &tree_root->rb_node;
	struct rb_node *parent = NULL;
	struct num_rb_entity *temp;

	while (*curr)
	{
		parent = *curr;
		temp = rb_entry(parent, struct num_rb_entity, node);
		if (entity->data < temp->data)
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

static void insert_into_rbtree(int *data, int length, struct rb_root *tree_root)
{
	int i = 0;
	struct num_rb_entity *entity;

	printk(KERN_INFO "Inserting into the Red-Black Tree...\n");
	for (i = 0; i < length; i++)
	{
		entity = kmalloc(sizeof(struct num_rb_entity), GFP_KERNEL);
		entity->data = data[i];
		insert(entity, tree_root);
	}
}

static struct num_rb_entity * lookup(int key, struct rb_root *tree_root)
{
	struct rb_node **curr = &tree_root->rb_node;
	struct rb_node *parent = NULL;
	struct num_rb_entity *temp;

	while (*curr)
	{
		parent = *curr;
		temp = rb_entry(parent, struct num_rb_entity, node);
		if (key < temp->data)
		{
			curr = &parent->rb_left;
		}
		else if (key > temp->data)
		{
			curr = &parent->rb_right;
		}
		else	/* key == temp->data */
		{
			return temp;
		}
	}
	return NULL;
}
static void lookup_rbtree(int *data, int length, struct rb_root *tree_root)
{
	int i;
	struct num_rb_entity *entity;
	
	printk(KERN_INFO "Looking up inserted integers in the Red-Black Tree...\n");
	for (i = 0; i < length; i++)
	{
		entity = lookup(data[i], tree_root);
		if (entity != NULL)
		{
			printk(KERN_INFO "Red-Black Tree: Entity found: %d\n", entity->data);
		}
		else
		{
			printk(KERN_INFO "Red-Black Tree: Entity %d Not found!\n", data[i]);
		}
	}
}

static void destruct_rbtree(struct rb_root *tree_root)
{
	struct rb_node *tmp_node, *next_node;
	struct num_rb_entity *tmp_entity;

	printk(KERN_INFO "Deleting entity from Red-Black Tree!\n");
	tmp_node = rb_first(tree_root);
	while(tmp_node)
	{
		next_node = rb_next(tmp_node);
		tmp_entity = rb_entry(tmp_node, struct num_rb_entity, node);
		rb_erase(tmp_node, tree_root);
		kfree(tmp_entity);
		tmp_node = next_node;
	}
}
/* Red-Black Tree methods - END */

/* Hash Table methods - START */
static void insert_into_hasht(int *data, int length, struct my_hashmap *hashm)
{
	int i;
	struct num_h_entity *entity;
	
	printk(KERN_INFO "Inserting into the Hash Table...\n");
	for (i = 0; i < length; i++)
	{
		entity = kmalloc(sizeof(struct num_h_entity), GFP_KERNEL);
		entity->data = data[i];
		hash_add(hashm->my_hasht, &entity->node, data[i]);
	}
}

static void iterate_hasht(struct my_hashmap *hashm)
{
	unsigned int bkt;
	struct num_h_entity *curr;
	
	printk(KERN_INFO "Iterating over the Hash Table...\n");
	hash_for_each(hashm->my_hasht, bkt, curr, node)
	{
		printk(KERN_INFO "Hash Table: Entity: %d\n", curr->data);
	}
}

static void lookup_in_hasht(int* data, int length, struct my_hashmap *hashm )
{
	int i;
	struct num_h_entity *curr;
	
	printk(KERN_INFO "Looking up entities in the Hash Table...\n");
	for (i = 0; i < length; i++)
	{
		hash_for_each_possible(hashm->my_hasht, curr, node, data[i])
		{
			printk(KERN_INFO "Hash Table: Lookup for key %d: %d\n", data[i], curr->data);
		}
	}
}

static void remove_from_hasht(struct my_hashmap *hashm)
{
	unsigned int bkt;
	struct num_h_entity *curr;
	
	printk(KERN_INFO "Deleting the inserted entities from the Hash Table...\n");
	hash_for_each(hashm->my_hasht, bkt, curr, node)
	{
		hash_del(&curr->node);
		kfree(curr);
	}
}

static void destruct_hasht(struct my_hashmap *hashm)
{
	printk(KERN_INFO "Destructing the Hash Table!\n");
	if (!hash_empty(hashm->my_hasht))
	{
		printk(KERN_INFO "First, removing all entities from the Hash Table...\n");
		remove_from_hasht(hashm);
	}
	kfree(hashm);
}
/* Hash Table methods - END */

/* Radix Tree methods - START */
static void insert_into_rdxtree(int *data, int length, struct radix_tree_root *rdx_root)
{
	int i, ret;
	struct num_rdx_entity *entity;

	printk(KERN_INFO "Inserting into the Radix Tree...\n");
	for (i = 0; i < length; i++)
	{
		entity = kmalloc(sizeof(struct num_rdx_entity), GFP_KERNEL);
		entity->data = data[i];
		ret = radix_tree_insert(rdx_root, (long) data[i], entity);
		if (ret != 0)
		{
			printk(KERN_ERR "Radix Tree: Error inserting %d, code: %d\n", data[i], ret);
		}
	}
}

static void lookup_rdxtree(int *data, int length, struct radix_tree_root *rdx_root)
{
	int i;
	struct num_rdx_entity *entity;

	printk(KERN_INFO "Looking up inserted entity from Radix Tree...\n");
	for (i = 0; i < length; i++)
	{
		entity = radix_tree_lookup(rdx_root, data[i]);
		if (entity == NULL)
		{
			printk(KERN_ERR "Radix Tree: Entity %d not found!\n", data[i]);
		}
		else
		{
			printk(KERN_INFO "Radix Tree: Entity found: %d\n", entity->data);
		}
	}
}

static void tag_odd_rdxtree(struct radix_tree_root *rdx_root)
{
	struct radix_tree_iter iter;
	void __rcu **slot;
	void *result;

	printk(KERN_INFO "Tagging odd entities in the Radix Tree...\n");
	for (slot = radix_tree_iter_init(&iter, 0); slot || (slot = radix_tree_next_chunk(rdx_root, &iter, 0)); slot = radix_tree_next_slot(slot, &iter, 0))
	{
		result = rcu_dereference_raw(*slot);
		if (!result)
			continue;
		if (radix_tree_is_internal_node(result))
		{
			slot = radix_tree_iter_retry(&iter);
			continue;
		}
		if (((struct num_rdx_entity *) result)->data & 1)
		{
			radix_tree_tag_set(rdx_root, ((struct num_rdx_entity *) result)->data, RDX_ODD_MARK);
		}
	}
}

static void lookup_tagged_rdxtree(struct radix_tree_root *rdx_root)
{
	int count, i;
	struct num_rdx_entity **entities;

	entities = (struct num_rdx_entity **) kmalloc(sizeof(struct num_rdx_entity *) * MAX_RDX_SIZE, GFP_KERNEL);
	printk(KERN_INFO "Fetching the odd marked entity from Radix Tree...\n");
	count = radix_tree_gang_lookup_tag(rdx_root, (void **) entities, 0, MAX_RDX_SIZE, RDX_ODD_MARK);
	for (i = 0; i < count; i++)
	{
		printk(KERN_INFO "Radix Tree: Fetched odd marked entity: %d\n", entities[i]->data);
	}
	kfree(entities);
}

static void remove_all_rdxtree(struct radix_tree_root *rdx_root)
{
	int count, i;
	struct num_rdx_entity **all_entities;

	all_entities = (struct num_rdx_entity **) kmalloc(sizeof(struct num_rdx_entity *) * MAX_RDX_SIZE, GFP_KERNEL);
	printk(KERN_INFO "Removing all entries from the Radix Tree...\n");
	count = radix_tree_gang_lookup(rdx_root, (void **) all_entities, 0, MAX_RDX_SIZE);
	for (i = 0; i < count; i++)
	{
		radix_tree_delete(rdx_root, all_entities[i]->data);
		kfree(all_entities[i]);
	}
}
/* Radix Tree methods - END */

/* XArray methods - START */
static void insert_into_xarr(int *data, int length, struct xarray *xarr)
{
	int i, ret;
	void *res;

	printk(KERN_INFO "Inserting into the XArray...\n");
	for (i = 0; i < length; i++)
	{
		res = xa_store(xarr, data[i], xa_mk_value(data[i]), GFP_KERNEL);
		ret = xa_err(res);
		if (ret != 0)
		{
			printk(KERN_ERR "XArray: Error inserting %d, code: %d\n", data[i], ret);
		}
	}
}

static void lookup_xarr(int *data, int length, struct xarray *xarr)
{
	int i, curr;
	void *tmp;

	printk(KERN_INFO "Looking up inserted values in XArray...\n");
	for (i = 0; i < length; i++)
	{
		tmp = xa_load(xarr, data[i]);
		if (tmp == NULL)
		{
			printk(KERN_ERR "XArray: Could not find entry for %d!\n", data[i]);
		}
		else
		{
			curr = xa_to_value(tmp);
			printk(KERN_INFO "XArray: Entity found: %d\n", curr);
		}
	}
}

static void tag_odd_xarr(struct xarray *xarr)
{
	unsigned long int idx;
	void *tmp;

	printk(KERN_INFO "Marking odd entities in the XArray...\n");
	xa_for_each(xarr, idx, tmp)
	{
		if (xa_to_value(tmp) & 1)
		{
			xa_set_mark(xarr, idx, XA_MARK_1);
		}
	}
}

static void lookup_tagged_xarr(struct xarray *xarr)
{
	unsigned long int idx;
	void *tmp;
	int curr;

	printk(KERN_INFO "Looking up odd tagged entities in XArray...\n");
	xa_for_each_marked(xarr, idx, tmp, XA_MARK_1)
	{
		curr = xa_to_value(tmp);
		printk(KERN_INFO "XArray: Tagged value: %d\n", curr);
	}
}

static void remove_all_xarr(struct xarray *xarr)
{
	unsigned long int idx;
	void *tmp;

	printk(KERN_INFO "Removing all inserted entities from XArray...!\n");
	xa_for_each(xarr, idx, tmp)
	{
		xa_erase(xarr, idx);
	}
}
/* XArray methods - END */

/* Bitmap methods - START */
static void set_in_bmap(int *data, int length, unsigned long *bmap)
{
	int i;

	printk(KERN_INFO "Setting bits corresponding to the received numbers...\n");
	for (i = 0; i < length; i++)
	{
		set_bit(data[i], bmap);
	}
}

static void print_set_bits_bmap(unsigned long *bmap)
{
	int bit;

	printk(KERN_INFO "Looking up all the set bits...\n");
	for_each_set_bit(bit, bmap, MAX_BMAP_ENTRY_NUM)
	{
		printk(KERN_INFO "Bitmap: Turned ON bit: %d\n", bit);
	}
}

static void clear_all_bits_bmap(unsigned long *bmap)
{
	printk(KERN_INFO "Clearing all the bits from bitmap!\n");
	bitmap_zero(bmap, MAX_BMAP_ENTRY_NUM);
}
/* Bitmap methods - END */

static int __init on_init(void)
{
	int s_len, i, ret;
	char *token;

	printk(KERN_INFO "Module loaded.\n");

	s_len = strlen(int_str);
	n_len = s_len / 2 + 1;	/* Max number of ints that can be in a space separated string of ints */

	input = (int *) kmalloc(sizeof(int) * n_len, GFP_KERNEL);
	if (input == NULL)
	{
		printk(KERN_ERR "Could not allocate memory for input array\n");
		return 1;
	}

	printk(KERN_INFO "Received values\n");
	for (i = 0; (token = strsep(&int_str, " ")) != NULL; i++)
	{
		if (!*token)
		{
			continue;
		}
		ret = kstrtoint(token, DECIMAL_BASE, &input[i]);
		if (ret < 0)
		{
			printk(KERN_ERR "Error in converting str to int for token %s with error code %d\n", token, ret);
			return ret;
		}

		printk(KERN_INFO "%d\n", input[i]);
	}
	n_len = i;
	printk(KERN_INFO "There are total %d numbers\n", n_len);

	/* Linked List */
	printk(KERN_INFO "\n\nLinked List-------START-------------\n\n");
	define_llist(input, n_len, &my_llist); 		/* Linked List (1) */
	iterate_llist(&my_llist);			/* Linked List (2) */
	destruct_llist(&my_llist);                      /* Linked List (3) */
	printk(KERN_INFO "\nLinked List-------END-------------\n\n"); 

	/* Red-Black Tree */
	printk(KERN_INFO "\n\nRed-Black Tree-------START-------------\n\n"); 
	insert_into_rbtree(input, n_len, &my_rbtree);	/* Red-Black Tree (2) */
	lookup_rbtree(input, n_len, &my_rbtree);	/* Red-Black Tree (3) */
	destruct_rbtree(&my_rbtree);                    /* Red-Black Tree (4) */
	printk(KERN_INFO "\nRed-Black Tree-------END-------------\n\n");

	/* Hash Table */
	printk(KERN_INFO "\n\nHash Table-------START-------------\n\n"); 
	my_hashm = kmalloc(sizeof(struct my_hashmap), GFP_KERNEL);	/* Hash Table (1) */
	hash_init(my_hashm->my_hasht);			/* Hash Table (1) */
	insert_into_hasht(input, n_len, my_hashm);	/* Hash Table (2) */
	iterate_hasht(my_hashm);			/* Hash Table (3) */
	lookup_in_hasht(input, n_len, my_hashm);	/* Hash Table (4) */
	remove_from_hasht(my_hashm);			/* Hash Table (5) */
	destruct_hasht(my_hashm);                       /* Hash Table (6) */
	printk(KERN_INFO "\nHash Table-------END-------------\n\n");

	/* Radix Tree */
	printk(KERN_INFO "\n\nRadix Tree-------START-------------\n\n");
	insert_into_rdxtree(input, n_len, &my_rdxtree);	/* Radix Tree (2) */
	lookup_rdxtree(input, n_len, &my_rdxtree);	/* Radix Tree (3) */
	tag_odd_rdxtree(&my_rdxtree);			/* Radix Tree (4) */
	lookup_tagged_rdxtree(&my_rdxtree);		/* Radix Tree (5) */
	remove_all_rdxtree(&my_rdxtree);                /* Radix Tree (6) */
	printk(KERN_INFO "\nRadix Tree-------END-------------\n\n");

	/* XArray */
	printk(KERN_INFO "\n\nXArray-------START-------------\n\n"); 
	insert_into_xarr(input, n_len, &my_xarray);	/* XArray (2) */
	lookup_xarr(input, n_len, &my_xarray);		/* XArray (3) */
	tag_odd_xarr(&my_xarray);			/* XArray (4) */
	lookup_tagged_xarr(&my_xarray);			/* XArray (5) */
	remove_all_xarr(&my_xarray);                    /* XArray (6) */
	printk(KERN_INFO "\nXArray-------END-------------\n\n");

	/* Bitmap */
	printk(KERN_INFO "\n\nBitmap-------START-------------\n\n"); 
	set_in_bmap(input, n_len, my_bmap);		/* Bitmap (2) */
	print_set_bits_bmap(my_bmap);			/* Bitmap (3) */
	clear_all_bits_bmap(my_bmap);                   /* Bitmap (4) */
	printk(KERN_INFO "\nBitmap-------END-------------\n\n");

	return 0;
}

static void __exit on_exit(void)
{
	kfree(input);

	printk(KERN_INFO "Exiting module.\n");
}

module_init(on_init);
module_exit(on_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Manish Adkar <madkar@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Project 2");

