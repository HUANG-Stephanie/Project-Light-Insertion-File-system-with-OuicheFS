// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/file.h>

#include "ouichefs.h"

#define MAX 4096
#define IOCTL_NAME "ouichefs_ioctl"
#define CMD _IOWR('N', 0 , char* )

/*
 * Mount a ouiche_fs partition
 */
struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *dentry = NULL;

	dentry =
		mount_bdev(fs_type, flags, dev_name, data, ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	kill_block_super(sb);

	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};


long ouichefs_ioctl(struct file* file, unsigned int cmd, unsigned long arg){
	pr_info("OUICHEFS IOCTL\n");
	
	(void)cmd;
	int fd = (int)arg;
	struct file *data = fget(fd);
	
        int nb_used_blocks = 0;
        int nb_partially_blocks = 0;
        int nb_bytes_wasted = 0;
        int delayed_bytes_wasted = 0;

        struct inode *inode = file_inode(data);
        struct super_block *sb = inode->i_sb;
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct ouichefs_inode *cinode = NULL;
        struct buffer_head *bh = NULL;
        struct buffer_head *bh2 = NULL;
        struct buffer_head *bh3 = NULL;
        struct ouichefs_file_index_block *file_block = NULL;
	uint32_t inode_block = (inode->i_ino / OUICHEFS_INODES_PER_BLOCK) + 1;
        uint32_t inode_shift = inode->i_ino % OUICHEFS_INODES_PER_BLOCK;
        uint32_t size_used, nb_block;
        	
        bh = sb_bread(sb, inode_block);
        if (!bh){
                return -EIO;
	}
	cinode = (struct ouichefs_inode *)(bh->b_data);
	cinode += inode_shift;
	
	pr_info("CI INDEX = %d\n", ci->index_block);	
	pr_info("CINODE INDEX = %d\n", cinode->index_block);	
	
        bh2 = sb_bread(sb, cinode->index_block);
        if (!bh2){
                return -EIO;
        }
        file_block = (struct ouichefs_file_index_block *)(bh2->b_data);
   
        pr_info("NB bloc inode = %llu\n", inode->i_blocks);
        pr_info("NB bloc cinode = %d\n", cinode->i_blocks);
	        
        for(int i = 0; i < inode->i_blocks; i++){
                pr_info("Bloc = %d\n", file_block->blocks[i]);
		                
                if(file_block->blocks[i] == 0){
                        pr_info("Termine\n");
                        break;
                }
	
		size_used = file_block->blocks[i];
        	nb_block = file_block->blocks[i];
        	// Nombre de données réel dans le bloc
                size_used = size_used >> 20;
                nb_block = nb_block & 0xFFFFF;
                
                pr_info("BLOC REEL = %d\n", nb_block);
                pr_info("TAILLE USED = %d\n", size_used);
                
                bh3 = sb_bread(sb, file_block->blocks[i]);
		if (!bh3){
		        return -EIO;
		}
		pr_info("DATA = %s\n",(char*)bh3->b_data);
 		brelse(bh3);
				  
                nb_bytes_wasted += delayed_bytes_wasted;
                nb_used_blocks++;
                if(size_used < MAX){
                        nb_partially_blocks++;
                        delayed_bytes_wasted += (MAX - size_used);
                }
                else{
                        delayed_bytes_wasted = 0;
                }
        }

        pr_info("Nombre de bloc utilisé = %d\n", nb_used_blocks);
        pr_info("Nombre de bloc partiellement utilisé = %d\n", nb_partially_blocks);
        pr_info("Nombre d'octets perdus du a la fragmentation = %d\n", nb_bytes_wasted);

        for(int i = 0; i < inode->i_blocks - 1; i++){
                
                if(!file_block->blocks[i]){
                        pr_info("Termine\n");
                        break;
                }
		
		size_used = file_block->blocks[i];
        	nb_block = file_block->blocks[i];
        	// Nombre de données réel dans le bloc
                size_used = size_used >> 20;
        	// Numero du bloc réel
                nb_block = nb_block & 0xFFFFF;

                pr_info("Numero de bloc = %d\nTaille du bloc effectif = %d\n", nb_block, size_used);
        }

        brelse(bh);
        brelse(bh2);        
        fput(data);
	return 0;
}


static struct file_operations ioctl_ops = {
	.unlocked_ioctl = ouichefs_ioctl,
};

static int major;
static int __init ouichefs_init(void)
{
	int ret;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto err;
	}

	ret = register_filesystem(&ouichefs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto err_inode;
	}
	
	major = register_chrdev(0, IOCTL_NAME, &ioctl_ops);
	if (major < 0) {
	    pr_err("Failed to register device\n");
	    return major;
	}
	pr_info("Numero du major : %d\n", major);
	
	pr_info("module loaded\n");
	return 0;

err_inode:
	ouichefs_destroy_inode_cache();
err:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();
	
	unregister_chrdev(major, IOCTL_NAME);

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@rwth-aachen.de>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
