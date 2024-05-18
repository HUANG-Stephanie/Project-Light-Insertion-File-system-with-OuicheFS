ssize_t ouichefs_read(struct file* file, char __user* user_buf, size_t size, loff_t* ppos){
        pr_info("OUICHEFS READ\n");
	pr_info("OFFSET = %lld\n", *ppos);
	
        struct buffer_head * bh;
        struct inode* inode = file->f_inode; 
        struct super_block *sb = inode->i_sb;

        // Numéro de bloc correspondant à l'offset
        sector_t nb_block = *ppos >> sb->s_blocksize_bits;
        
        pr_info("Num block = %llu\n", nb_block);
        
        // Lecture du bloc
        bh = sb_bread(sb, nb_block);
        if (!bh){
		return -EIO;
        }

	pr_info("DONNEE BLOC = %s\n", bh->b_data + (*ppos % sb->s_blocksize));

        // Calcul longueur max qu'on peut lire dans le bloc
        size_t bh_size = min(size, (size_t)(sb->s_blocksize - (*ppos % sb->s_blocksize)));
        
        pr_info("SIZE = %lu\n", bh_size);
        
        // Copie vers l'utilisateur
        if(copy_to_user(user_buf, bh->b_data + (*ppos % sb->s_blocksize), bh_size)){
                brelse(bh);
                return -EFAULT;
        }
        // Mis à jour de la position dans le fichier
        file->f_pos += bh_size;
        
        pr_info("DONNEE USER = %s\n", user_buf);
	
        // Libératon du bloc
        brelse(bh);
	pr_info("POSITION FIN = %lld\n", file->f_pos);
        return bh_size;
}

ssize_t ouichefs_write(struct file* file, const char __user* user_buf, size_t size, loff_t* ppos){
        pr_info("OUICHEFS WRITE\n");
        pr_info("OFFSET = %lld\n", *ppos);
                
        char* data;
        size_t copy_bytes = 0;
        struct buffer_head * bh;
        struct inode* inode = file->f_inode;
        struct super_block *sb = inode->i_sb;

        // Numéro de bloc correspondant à l'offset
        sector_t nb_block = *ppos >> sb->s_blocksize_bits;

        // Position de l'offset dans le bloc 
        unsigned offset = *ppos % sb->s_blocksize;
        
        // Ecriture dans plusieurs blocs
        while(copy_bytes < size){
		
		pr_info("SIZE = %lu\n", copy_bytes);
		
                // Calcul len max à ecrire dans le bloc
                size_t len = min(sb->s_blocksize - offset, size - copy_bytes);

		pr_info("LEN = %zu\n", len);

                bh = sb_bread(sb, nb_block);
                if (!bh){
		        return -EIO;
                }

                // Ecriture à la fin des données precédentes
                data = bh->b_data + offset;
                
                pr_info("DATA A ECRIRE AVANT = %s\n", bh->b_data + offset);		
                
                if(copy_from_user(data, user_buf + copy_bytes, len)){
                        brelse(bh);
                        return -EFAULT;
                }
                
                pr_info("DATA A ECRIRE APRES = %s\n", bh->b_data + offset);		
                pr_info("DONNEE USER = %s\n", user_buf);
		
                // Marquer dirty et forcer l'écriture sur disque
                mark_buffer_dirty(bh);
                sync_dirty_buffer(bh);
                brelse(bh);
                
                /* Incrémenter la longueur de donnée ecrite
                Incrémenter l'offset initial
                Passer au bloc suivant car fin du bloc courant
                offset à 0 car nouveau bloc 
                */
                copy_bytes += len;
                if(offset + len >= sb->s_blocksize){
		        nb_block++;
		        inode->i_blocks++;
		        offset = 0;
               	}
                
        }
        inode->i_size += copy_bytes;
        file->f_pos += copy_bytes;
        pr_info("POSITION FIN = %lld\n", file->f_pos);
        return copy_bytes;
}