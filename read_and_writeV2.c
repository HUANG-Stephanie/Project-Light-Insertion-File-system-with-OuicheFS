ssize_t ouichefs_read(struct file* file, char __user* user_buf, size_t size, loff_t* ppos){
        pr_info("OUICHEFS READ\n");
	    pr_info("OFFSET = %lld\n", *ppos);
	
        struct inode *inode = file_inode(file);
        struct super_block *sb = inode->i_sb;
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct buffer_head *bh = NULL;
        struct buffer_head *bh2 = NULL;
        struct ouichefs_file_index_block *file_block = NULL;

        // Recuperer le tableau des blocs
        bh = sb_bread(sb, ci->index_block);
        if (!bh)
                return -EIO;
        file_block = (struct ouichefs_file_index_block *)bh->b_data;

        // Numéro de bloc correspondant à l'offset
        int offset_block = (*ppos >> sb->s_blocksize_bits);
        
        pr_info("NUM BLOC = %d\n", nb_block);

        size_t lu = 0;
        while(lu < size && file_block->blocks[offset_block] != 0){

                // Nombre de données réel dans le bloc
                uint32_t size_used = file_block->blocks[offset_block] >> 20;
                // Numero du bloc réel
                uint32_t nb_block = file_block->blocks[offset_block] & 0xFFFFF;
                // Lecture du bloc
                bh2 = sb_bread(sb, nb_block);
                if (!bh2){
                    return -EIO;
                }

                pr_info("DONNEE BLOC = %s\n", bh2->b_data + (*ppos % sb->s_blocksize));

                // Calcul longueur max qu'on peut lire dans le bloc
                size_t bh_size = min(size - lu, (size_t)(size_used - (*ppos % sb->s_blocksize)));
                
                pr_info("SIZE = %lu\n", bh_size);

                // Copie vers l'utilisateur
                if(copy_to_user(user_buf, bh2->b_data + (*ppos % sb->s_blocksize), bh_size)){
                        brelse(bh);
                        return -EFAULT;
                }
                // Mis à jour de la position dans le fichier
                file->f_pos += bh_size;

                pr_info("DONNEE USER = %s\n", user_buf);

                // Libératon du bloc
                brelse(bh2);

                lu += bh_size;
                // Passe au bloc suivant
                offset_block++;
        }

        // Libératon du bloc
        brelse(bh);
	    pr_info("POSITION FIN = %lld\n", file->f_pos);
        return lu;
}

ssize_t ouichefs_write(struct file* file, const char __user* user_buf, size_t size, loff_t* ppos){
        pr_info("OUICHEFS WRITE\n");
        pr_info("OFFSET = %lld\n", *ppos);
                
        struct inode *inode = file_inode(file);
        struct super_block *sb = inode->i_sb;
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
        struct buffer_head *bh = NULL;
        struct buffer_head *bh2 = NULL;
        struct buffer_head *bh3 = NULL;
        struct ouichefs_file_index_block *file_block = NULL;
        uint32_t *new_block, size_used, nb_block, num_block;
        char *data, buf;
        size_t written_data = 0, len = 0;

        // Recuperer le tableau des blocs
        bh = sb_bread(sb, ci->index_block);
        if (!bh){
                return -EIO;
        }
        file_block = (struct ouichefs_file_index_block *)bh->b_data;

        // Numéro de bloc correspondant à l'offset
        sector_t offset_block = (*ppos >> sb->s_blocksize_bits);
        
        pr_info("NUM BLOC = %d\n", offset_block);

        // Position de l'offset dans le bloc 
        unsigned offset = *ppos % sb->s_blocksize;

        // Nombre de données réel dans le bloc
        size_used = file_block->blocks[offset_block] >> 20;
        // Numero du bloc réel
        nb_block = file_block->blocks[offset_block] & 0xFFFFF;

        bh2 = sb_bread(sb, nb_block);
        if (!bh2){
                return -EIO;
        }

        data = bh2->b_data + offset;
        // Buffer qui contient les donnees après l'offset
        size_t size_from_offset = sb->s_blocksize - offset;
        buf = kmalloc(size_from_offset, GFP_KERNEL);
        memcpy(buf, data, size_from_offset);

        while(written_data < size){

                // Calcul len max à ecrire dans le bloc
                len = min(sb->s_blocksize - offset, size - written_data);

                pr_info("LEN = %zu\n", len);
                
                if(len == 0){

                        // Allocation nouveau bloc
                        new_block = get_free_block(sbi);
                        if (!new_block) {
                                return -ENOSPC;
                        }
                        bh3 = sb_bread(sb, new_block);
                        if (!bh3){
                                return -EIO;
                        }

                        // Copie des données depuis le user
                        if(copy_from_user(bh3->b_data, user_buf + written_data, min(sb->s_blocksize, size - written_data))){
                            brelse(bh);
                            brelse(bh2);
                            brelse(bh3);
                            return -EFAULT;
                        }
                        mark_buffer_dirty(bh3);
                        sync_dirty_buffer(bh3);

                        // Calcul du numero de bloc
                        num_block = (min(sb->s_blocksize, size - written_data) << 20) | new_block & 0xFFFFF;

                        written_data += len;

                        // Decalage de tous les blocs
                        for(int i = ci->i_blocks; i>offset_block; i--){
                            file_block->blocks[i] = file_block->blocks[i-1];
                        }
                        offset_block++;
                        file_block->blocks[offset_block] = num_block;

                        // Mis a jour de l'inode
                        ci->i_blocks++;

                        offset = min(sb->s_blocksize, size - written_data);
                        brelse(bh3);
                }
                else{
                        // Copie des données depuis le user
                        if(copy_from_user(data, user_buf + written_data, len)){
                            brelse(bh);
                            brelse(bh2);
                            return -EFAULT;
                        }
                        mark_buffer_dirty(bh2);
                        sync_dirty_buffer(bh2);

                        // Calcul du numero de bloc
                        num_block = ((size_used + len) << 20) | nb_block & 0xFFFFF;
                        file_block->blocks[offset_block] = num_block;

                        written_data += len;
                        offset += len;
                }

        }

        // Ajout donnée du buffer

        // Nombre de données réel dans le bloc
        size_used = file_block->blocks[offset_block] >> 20;
        // Numero du bloc réel
        nb_block = file_block->blocks[offset_block] & 0xFFFFF;

        if(sb->s_blocksize - size_used >= size_from_offset){
                bh3 = sb_bread(sb, nb_block);
                if (!bh3){
                        return -EIO;
                }
                memcpy(bh3->b_data + size_used, buf, size_from_offset);
        }
        else{
                // Allocation nouveau bloc
                new_block = get_free_block(sbi);
                if (!new_block) {
                        return -ENOSPC;
                }
                bh3 = sb_bread(sb, new_block);
                if (!bh3){
                        return -EIO;
                }
                memcpy(bh3->b_data, buf, size_from_offset);
        }

        brelse(bh);
        brelse(bh2);
        brelse(bh3);
        kfree(buf);
        
        // Mis à jour de la position dans le fichier
        file->f_pos += written_data;
        ci->size += written_data;
        
        return written_data;
}