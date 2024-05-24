// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>

#include "ouichefs.h"
#include "bitmap.h"

/*
 * Map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true, allocate a new block on disk and map it.
 */
static int ouichefs_file_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	int ret = 0, bno;

	/* If block number exceeds filesize, fail */
	if (iblock >= OUICHEFS_BLOCK_SIZE >> 2)
		return -EFBIG;

	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/*
	 * Check if iblock is already allocated. If not and create is true,
	 * allocate it. Else, get the physical block number.
	 */
	if (index->blocks[iblock] == 0) {
		if (!create) {
			ret = 0;
			goto brelse_index;
		}
		bno = get_free_block(sbi);
		if (!bno) {
			ret = -ENOSPC;
			goto brelse_index;
		}
		index->blocks[iblock] = bno;
	} else {
		bno = index->blocks[iblock];
	}

	/* Map the physical block to the given buffer_head */
	map_bh(bh_result, sb, bno);

brelse_index:
	brelse(bh_index);

	return ret;
}

/*
 * Called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static void ouichefs_readahead(struct readahead_control *rac)
{
	mpage_readahead(rac, ouichefs_file_get_block);
}

/*
 * Called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int ouichefs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, ouichefs_file_get_block, wbc);
}

/*
 * Called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int ouichefs_write_begin(struct file *file,
				struct address_space *mapping, loff_t pos,
				unsigned int len, struct page **pagep,
				void **fsdata)
{
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	int err;
	uint32_t nr_allocs = 0;

	/* Check if the write can be completed (enough space?) */
	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max(pos + len, file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	/* prepare the write */
	err = block_write_begin(mapping, pos, len, pagep,
				ouichefs_file_get_block);
	/* if this failed, reclaim newly allocated blocks */
	if (err < 0) {
		pr_err("%s:%d: newly allocated blocks reclaim not implemented yet\n",
		       __func__, __LINE__);
	}
	return err;
}

/*
 * Called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int ouichefs_write_end(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned int len, unsigned int copied,
			      struct page *page, void *fsdata)
{
	int ret;
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;

	/* Complete the write() */
	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	if (ret < len) {
		pr_err("%s:%d: wrote less than asked... what do I do? nothing for now...\n",
		       __func__, __LINE__);
	} else {
		uint32_t nr_blocks_old = inode->i_blocks;

		/* Update inode metadata */
		inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);

		/* If file is smaller than before, free unused blocks */
		if (nr_blocks_old > inode->i_blocks) {
			int i;
			struct buffer_head *bh_index;
			struct ouichefs_file_index_block *index;

			/* Free unused blocks from page cache */
			truncate_pagecache(inode, inode->i_size);

			/* Read index block to remove unused blocks */
			bh_index = sb_bread(sb, ci->index_block);
			if (!bh_index) {
				pr_err("failed truncating '%s'. we just lost %llu blocks\n",
				       file->f_path.dentry->d_name.name,
				       nr_blocks_old - inode->i_blocks);
				goto end;
			}
			index = (struct ouichefs_file_index_block *)
					bh_index->b_data;

			for (i = inode->i_blocks - 1; i < nr_blocks_old - 1;
			     i++) {
				put_block(OUICHEFS_SB(sb), index->blocks[i]);
				index->blocks[i] = 0;
			}
			mark_buffer_dirty(bh_index);
			brelse(bh_index);
		}
	}
end:
	return ret;
}

const struct address_space_operations ouichefs_aops = {
	.readahead = ouichefs_readahead,
	.writepage = ouichefs_writepage,
	.write_begin = ouichefs_write_begin,
	.write_end = ouichefs_write_end
};

static int ouichefs_open(struct inode *inode, struct file *file) {
	bool wronly = (file->f_flags & O_WRONLY) != 0;
	bool rdwr = (file->f_flags & O_RDWR) != 0;
	bool trunc = (file->f_flags & O_TRUNC) != 0;

	if ((wronly || rdwr) && trunc && (inode->i_size != 0)) {
		struct super_block *sb = inode->i_sb;
		struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
		struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
		struct ouichefs_file_index_block *index;
		struct buffer_head *bh_index;
		sector_t iblock;

		/* Read index block from disk */
		bh_index = sb_bread(sb, ci->index_block);
		if (!bh_index)
			return -EIO;
		index = (struct ouichefs_file_index_block *)bh_index->b_data;

		for (iblock = 0; index->blocks[iblock] != 0; iblock++) {
			put_block(sbi, index->blocks[iblock]);
			index->blocks[iblock] = 0;
		}
		inode->i_size = 0;
		inode->i_blocks = 0;

		brelse(bh_index);
	}
	
	return 0;
}


ssize_t ouichefs_read(struct file* file, char __user* user_buf, size_t size, loff_t* ppos){
       	pr_info("OUICHEFS READ\n");
	pr_info("OFFSET = %lld\n", *ppos);
	
        struct inode *inode = file_inode(file);
        struct super_block *sb = inode->i_sb;
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct buffer_head *bh = NULL;
        struct buffer_head *bh2 = NULL;
        struct ouichefs_file_index_block *file_block = NULL;
        uint32_t size_used, nb_block;

        // Recuperer le tableau des blocs
        bh = sb_bread(sb, ci->index_block);
        if (!bh){
                return -1;
        }
        
        file_block = (struct ouichefs_file_index_block *)bh->b_data;

        // Numéro de bloc correspondant à l'offset
        sector_t offset_block = (*ppos >> sb->s_blocksize_bits);
        
        pr_info("NUM BLOC = %llu\n", offset_block);
        
        pr_info("BLOC DANS FILE_BLOCK = %d\n", file_block->blocks[offset_block]);
        pr_info("BLOC 0 DANS FILE_BLOCK = %d\n", file_block->blocks[0]);
        pr_info("BLOC 1 DANS FILE_BLOCK = %d\n", file_block->blocks[1]);
        pr_info("BLOC 2 DANS FILE_BLOCK = %d\n", file_block->blocks[2]);
        
        // Position de l'offset dans le bloc 
        unsigned offset = *ppos % sb->s_blocksize;
        
       	char *buf = kmalloc(size, GFP_KERNEL);
       	memset(buf, 0, size);
        size_t lu = 0;
        while(lu < size && file_block->blocks[offset_block] != 0){

		size_used = file_block->blocks[offset_block];
        	nb_block = file_block->blocks[offset_block];
                // Nombre de données réel dans le bloc
                size_used = TAILLE_BLOCK(size_used);
                // Numero du bloc réel
                nb_block = NB_BLOCK(nb_block);
                
                pr_info("TAILLE DES DONNEE = %d\n", size_used);
                pr_info("BLOC REEL = %d\n", nb_block);
                
                // Lecture du bloc
                bh2 = sb_bread(sb, nb_block);
                if (!bh2){
                    return -1;
                }

                pr_info("DONNEE BLOC = %s\n", bh2->b_data + offset);

                // Calcul longueur max qu'on peut lire dans le bloc
                size_t bh_size = min(size - lu, (size_t)(sb->s_blocksize - offset));
                
                pr_info("SIZE = %lu\n", bh_size);

		memcpy(buf + lu, bh2->b_data + offset, bh_size);

                // Mis à jour de la position dans le fichier
                file->f_pos += bh_size;

                // Libératon du bloc
                brelse(bh2);

                lu += bh_size;
                // Passe au bloc suivant
                offset_block++;
                offset = 0;
                
                pr_info("LU = %lu\n", lu);
        }
	
	// Copie vers l'utilisateur
        if(copy_to_user(user_buf, buf, lu)){
                brelse(bh);
                kfree(buf);
                return -1;
        }
	
	pr_info("DONNEE USER = %s\n", user_buf);
	
	pr_info("BLOC 0 DANS FILE_BLOCK 2 = %d\n", file_block->blocks[0]);
        pr_info("BLOC 1 DANS FILE_BLOCK 2 = %d\n", file_block->blocks[1]);
        pr_info("BLOC 2 DANS FILE_BLOCK 2 = %d\n", file_block->blocks[2]);
        
        bh2 = sb_bread(sb, NB_BLOCK(file_block->blocks[0]));
        if (!bh2){
                return -1;
        }
        pr_info("Bloc 1 -> contenue = %s\n", bh2->b_data);
        brelse(bh2);
	
	bh2 = sb_bread(sb, NB_BLOCK(file_block->blocks[1]));
        if (!bh2){
                return -1;
        }
        pr_info("Bloc 1 -> contenue = %s\n", bh2->b_data);
        brelse(bh2);
        
        bh2 = sb_bread(sb, NB_BLOCK(file_block->blocks[2]));
        if (!bh2){
                return -1;
        }
        pr_info("Bloc 1 -> contenue = %s\n", bh2->b_data);
        brelse(bh2);
        
	
        // Libératon du bloc
        brelse(bh);
        kfree(buf);
	pr_info("POSITION FIN = %lld\n", file->f_pos);
        return lu;
}

ssize_t ouichefs_write(struct file* file, const char __user* user_buf, size_t size, loff_t* ppos){
        pr_info("OUICHEFS WRITE\n");
        pr_info("OFFSET = %lld\n", *ppos);
                
        struct inode *inode = file_inode(file);
        struct super_block *sb = inode->i_sb;
        struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct inode *vfs_inode = &ci->vfs_inode;
        struct buffer_head *bh = NULL;
        struct buffer_head *bh2 = NULL;
        struct buffer_head *bh3 = NULL;
        struct ouichefs_file_index_block *file_block = NULL;
        uint32_t new_block, size_used, nb_block, num_block;
        char *data, *buf;
        size_t written_data = 0, len = 0;
        
        // Recuperer le tableau des blocs
        bh = sb_bread(sb, ci->index_block);
        if (!bh){
                return -1;
        }
        
        file_block = (struct ouichefs_file_index_block *)bh->b_data;

        // Numéro de bloc correspondant à l'offset
        sector_t offset_block = (*ppos >> sb->s_blocksize_bits);
        
        pr_info("NUM BLOC = %llu\n", offset_block);

	pr_info("BLOC OFFSET_BLOCK DANS FILE_BLOCK = %d\n", file_block->blocks[offset_block]);
	pr_info("BLOC 0 DANS FILE_BLOCK = %d\n", file_block->blocks[0]);
        pr_info("BLOC 1 DANS FILE_BLOCK = %d\n", file_block->blocks[1]);
        pr_info("BLOC 2 DANS FILE_BLOCK = %d\n", file_block->blocks[2]);

        // Bloc vide à l'emplacement de l'offset
        // Allocation de bloc au cas ou les précédents ne le sont pas
        for(int i = offset_block; i>=0; i--){
                if(file_block->blocks[offset_block] == 0){
                        // Allocation nouveau bloc
                        new_block = get_free_block(sbi);
                        if (!new_block) {
                                return -1;
                        }
                        bh3 = sb_bread(sb, new_block);
                        if (!bh3){
                                return -1;
                        }
                        memset(bh3->b_data, 0, sb->s_blocksize);
                        mark_buffer_dirty(bh3);
                        sync_dirty_buffer(bh3);
                        brelse(bh3);

                        // Calcul du numero de bloc
                        num_block = NB_BLOCK_WITH_SIZE(new_block, sb->s_blocksize);
                        
                        pr_info("num_block 0 = %d\n", num_block);
                        pr_info("size_used 0 = %lu\n", sb->s_blocksize << 20);
                       	pr_info("size_used 0 bis = %lu\n", TAILLE_BLOCK(sb->s_blocksize));
			pr_info("NUM bloc 0 = %d\n", new_block);
                                                
                        file_block->blocks[offset_block] = num_block;
                }
        }

        // Position de l'offset dans le bloc 
        unsigned offset = *ppos % sb->s_blocksize;
 
        size_used = file_block->blocks[offset_block];
        nb_block = file_block->blocks[offset_block];
        // Nombre de données réel dans le bloc
        size_used = TAILLE_BLOCK(size_used); 
        // Numero du bloc réel
        nb_block = NB_BLOCK(nb_block);
        
        pr_info("TAILLE DES DONNEE 1 = %d\n", size_used);
        pr_info("BLOC REEL 1 = %d\n", nb_block);

        bh2 = sb_bread(sb, nb_block);
        if (!bh2){
                return -1;
        }

        data = bh2->b_data + offset;
        // Buffer qui contient les donnees après l'offset
        size_t size_from_offset = sb->s_blocksize - offset;
        buf = kmalloc(size_from_offset, GFP_KERNEL);
        memcpy(buf, data, size_from_offset);
        pr_info("BUF = %s\n", buf);
        pr_info("BUF TAILLE = %lu\n", strlen(buf));
        
        while(written_data < size){

                // Calcul len max à ecrire dans le bloc
                len = min(sb->s_blocksize - offset, size - written_data);

                pr_info("LEN = %zu\n", len);
                pr_info("WRITTEN DATA = %lu\n", written_data);
                pr_info("OFFSET = %d\n", offset);
                
                if(len == 0){

                        // Allocation nouveau bloc
                        new_block = get_free_block(sbi);
                        if (!new_block) {
                                return -1;
                        }
                        bh3 = sb_bread(sb, new_block);
                        if (!bh3){
                                return -1;
                        }
			memset(bh3->b_data, 0, sb->s_blocksize);
                        
                        int size_new_block = min(sb->s_blocksize, size - written_data);
                        
                        // Copie des données depuis le user
                        if(copy_from_user(bh3->b_data, user_buf + written_data, size_new_block)){
                            brelse(bh);
                            brelse(bh2);
                            brelse(bh3);
                            return -1;
                        }
                        mark_buffer_dirty(bh3);
                        sync_dirty_buffer(bh3);
                        brelse(bh3);

                        // Calcul du numero de bloc
                        num_block = NB_BLOCK_WITH_SIZE(new_block, size_new_block); 

			pr_info("NUM bloc 1 = %d\n", num_block);

                        // Decalage de tous les blocs
                        for(int i = vfs_inode->i_blocks; i>offset_block; i--){
                                file_block->blocks[i] = file_block->blocks[i-1];
                                //file_block->blocks[i-1] = 0;
                        }
                        
                        offset_block++;
                        file_block->blocks[offset_block] = num_block;

                        // Mis a jour de l'inode
                        vfs_inode->i_blocks++;

                        written_data += size_new_block;
                        offset = size_new_block;
                        
                        pr_info("WRITTEN DATA 2 = %lu\n", written_data);
                }
                else{
                        // Copie des données depuis le user
                        if(copy_from_user(data, user_buf + written_data, len)){
                            brelse(bh);
                            brelse(bh2);
                            return -1;
                        }
                        mark_buffer_dirty(bh2);
                        sync_dirty_buffer(bh2);
     			
     			pr_info("USER CONTENU = %s\n", user_buf);
     			pr_info("DATA 4 CONTENU = %s\n", data);
     			     			
     			// Ajout de padding entre size_used et offset
                	int size_used_to_offset =  offset - size_used;
     			int size_current_block = size_used + len;
			if(size_used_to_offset > 0){
				size_current_block += size_used_to_offset;
			}
			pr_info("TAILLE DES DONNEE 4 = %d\n", size_used);
                	pr_info("size_used_to_offset = %d\n", size_used_to_offset);
                	
                        // Calcul du numero de bloc
                        num_block = NB_BLOCK_WITH_SIZE(nb_block, size_current_block);
                        file_block->blocks[offset_block] = num_block;
			
			pr_info("size_used 2 = %d\n", size_current_block);
			pr_info("NUM bloc 2 = %d\n", num_block);

                        written_data += len;
                        offset += len;
                }

	}

        // Ajout donnée du buffer
	
	size_used = file_block->blocks[offset_block];
        nb_block = file_block->blocks[offset_block];
        // Nombre de données réel dans le bloc
        size_used = TAILLE_BLOCK(size_used); 
        // Numero du bloc réel
        nb_block = NB_BLOCK(nb_block);

	pr_info("TAILLE DES DONNEE 2= %d\n", size_used);
        pr_info("BLOC REEL 2= %d\n", nb_block);
        
        pr_info("GAP = %lu\n", size_from_offset);

        
        if(sb->s_blocksize - size_used >= strlen(buf)){
        	pr_info("ASSEZ DE PLACE\n");
        	
                bh3 = sb_bread(sb, nb_block);
                if (!bh3){
                        return -1;
                }
                memcpy(bh3->b_data + (*ppos % sb->s_blocksize) +len, buf, (size_t)strlen(buf));
                mark_buffer_dirty(bh3);
                sync_dirty_buffer(bh3);
                brelse(bh3);
                
                pr_info("DATA BUF = %s\n", bh3->b_data);
                
        }
        else{
        	pr_info("PAS ASSEZ DE PLACE\n");
                // Allocation nouveau bloc
                new_block = get_free_block(sbi);
                if (!new_block) {
                        return -1;
                }
                bh3 = sb_bread(sb, new_block);
                if (!bh3){
                        return -1;
                }
                memset(bh3->b_data, 0, sb->s_blocksize);
                memcpy(bh3->b_data, buf, (size_t)strlen(buf));
                mark_buffer_dirty(bh3);
                sync_dirty_buffer(bh3);
                brelse(bh3);
                
                // Calcul du numero de bloc
                num_block = NB_BLOCK_WITH_SIZE(new_block, strlen(buf)); 
		
		// Decalage de tous les blocs
                for(int i = vfs_inode->i_blocks; i>offset_block; i--){
                    file_block->blocks[i] = file_block->blocks[i-1];
                    //file_block->blocks[i-1] = 0;
                }
                offset_block++;
                file_block->blocks[offset_block] = num_block;

                // Mis a jour de l'inode
                vfs_inode->i_blocks++;
		
        }
        
        pr_info("BLOC 0 DANS FILE_BLOCK 2 = %d\n", file_block->blocks[0]);
        pr_info("BLOC 1 DANS FILE_BLOCK 2 = %d\n", file_block->blocks[1]);
        pr_info("BLOC 2 DANS FILE_BLOCK 2 = %d\n", file_block->blocks[2]);
        
        bh3 = sb_bread(sb, NB_BLOCK(file_block->blocks[0]));
        if (!bh3){
                return -1;
        }
        pr_info("Bloc 1 -> contenue = %s\n", bh3->b_data);
        brelse(bh3);
	
	bh3 = sb_bread(sb, NB_BLOCK(file_block->blocks[1]));
        if (!bh3){
                return -1;
        }
        pr_info("Bloc 1 -> contenue = %s\n", bh3->b_data);
        brelse(bh3);
        
        bh3 = sb_bread(sb, NB_BLOCK(file_block->blocks[2]));
        if (!bh3){
                return -1;
        }
        pr_info("Bloc 1 -> contenue = %s\n", bh3->b_data);
        brelse(bh3);
	
        brelse(bh);
        brelse(bh2);
        kfree(buf);
        
        // Mis à jour de la position dans le fichier
        file->f_pos += written_data;
        vfs_inode->i_size += written_data;
        
        pr_info("i_size = %llu\n", vfs_inode->i_size);
        
        return written_data;
}


const struct file_operations ouichefs_file_ops = {
	.owner = THIS_MODULE,
	.open = ouichefs_open,
	.llseek = generic_file_llseek,
	.read_iter = generic_file_read_iter,
	.write_iter = generic_file_write_iter, 
	.read = ouichefs_read,
	.write = ouichefs_write,
};
