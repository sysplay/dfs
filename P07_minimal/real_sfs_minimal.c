/* Real Simula File System Module */
#include <linux/module.h> /* For module related macros, ... */
#include <linux/kernel.h> /* For printk, ... */
#include <linux/version.h> /* For LINUX_VERSION_CODE & KERNEL_VERSION */
#include <linux/fs.h> /* For system calls, structures, ... */
#include <linux/errno.h> /* For error codes */
#include <linux/slab.h> /* For kzalloc, ... */

#include "real_sfs_ds.h" /* For SFS related defines, data structures, ... */
#include "real_sfs_ops.h" /* For SFS related operations */

/*
 * Data declarations
 */
static struct file_system_type sfs;
static struct super_operations sfs_sops;
static struct inode_operations sfs_iops;
static struct file_operations sfs_fops;
static struct address_space_operations sfs_aops;

static struct inode *sfs_root_inode;

/*
 * File-System Supporting Operations
 */
static int get_bit_pos(unsigned int val)
{
	int i;

	for (i = 0; val; i++)
	{   
		val >>= 1;
	}   
	return (i - 1); 
}
static int sfs_fill_super(struct super_block *sb, void *data, int silent)
{
	sfs_info_t *info;

	printk(KERN_INFO "sfs: sfs_fill_super\n");
	if (!(info = (sfs_info_t *)(kzalloc(sizeof(sfs_info_t), GFP_KERNEL))))
		return -ENOMEM;
	info->vfs_sb = sb;
	if (init_browsing(info) < 0)
	{
		kfree(info);
		return -EIO;
	}
	/* Updating the VFS super_block */
	sb->s_magic = 0; /* TODO 1: File System Type extracted from our superblock */
	sb->s_blocksize = 0; /* TODO 2A: File System Block Size extracted from our superblock */
	sb->s_blocksize_bits = get_bit_pos(0 /* TODO 2B: File System Block Size */);
	sb->s_type = &sfs; // file_system_type
	sb->s_op = &sfs_sops; // super block operations

	// Obtain an inode from VFS
	sfs_root_inode = iget_locked(sb, 0 /* TODO 3: Root Inode Number as per our translation logic*/);
	if (!sfs_root_inode)
	{
		shut_browsing(info);
		kfree(info);
		return -EACCES;
	}
	if (sfs_root_inode->i_state & I_NEW) // allocated fresh now
	{
		printk(KERN_INFO "sfs: Got new root inode, let's fill in\n");
		sfs_root_inode->i_op = &sfs_iops; // inode operations
		sfs_root_inode->i_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		sfs_root_inode->i_fop = &sfs_fops; // file operations
		sfs_root_inode->i_mapping->a_ops = &sfs_aops; // address operations
		unlock_new_inode(sfs_root_inode);
	}
	else
	{
		printk(KERN_INFO "sfs: Got root inode from inode cache\n");
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
	sb->s_root = d_alloc_root(sfs_root_inode);
#else
	sb->s_root = d_make_root(sfs_root_inode);
#endif
	if (!sb->s_root)
	{
		iget_failed(sfs_root_inode);
		shut_browsing(info);
		kfree(info);
		return -ENOMEM;
	}

	return 0;
}

/*
 * File-System Operations
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
static int sfs_get_sb(struct file_system_type *fs_type, int flags, const char *devname, void *data, struct vfsmount *vm)
{
	printk(KERN_INFO "sfs: devname = %s\n", devname);

	 /* sfs_fill_super this will be called to fill the super block */
	return get_sb_bdev(fs_type, flags, devname, data, &sfs_fill_super, vm);
}
#else
static struct dentry *sfs_mount(struct file_system_type *fs_type, int flags, const char *devname, void *data)
{
	printk(KERN_INFO "sfs: devname = %s\n", devname);

	 /* sfs_fill_super this will be called to fill the super block */
	return mount_bdev(fs_type, flags, devname, data, &sfs_fill_super);
}
#endif

static void sfs_kill_sb(struct super_block *sb)
{
	sfs_info_t *info = (sfs_info_t *)(sb->s_fs_info);

	kill_block_super(sb);
	if (info)
	{
		shut_browsing(info);
		kfree(info);
	}
}

static struct file_system_type sfs =
{
	name: "sfs", /* Name of our file system */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38))
	get_sb:  sfs_get_sb,
#else
	mount:  sfs_mount,
#endif
	kill_sb: sfs_kill_sb,
	owner: THIS_MODULE
};

static int __init sfs_init(void)
{
	int err;

	err = register_filesystem(&sfs);
	return err;
}

static void __exit sfs_exit(void)
{
	unregister_filesystem(&sfs);
}

module_init(sfs_init);
module_exit(sfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email@sarika-pugs.com>");
MODULE_DESCRIPTION("File System Module for real Simula File System");
