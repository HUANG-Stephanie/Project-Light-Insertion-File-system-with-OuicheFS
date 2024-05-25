ssize_t ouichefs_read(struct file* file, char __user* user_buf, size_t size, loff_t* ppos){
	
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
                
                // Lecture du bloc
                bh2 = sb_bread(sb, nb_block);
                if (!bh2){
                    return -1;
                }

                // Calcul longueur max qu'on peut lire dans le bloc
                size_t bh_size = min(size - lu, (size_t)(sb->s_blocksize - offset));

		memcpy(buf + lu, bh2->b_data + offset, bh_size);

                // Mis à jour de la position dans le fichier
                file->f_pos += bh_size;

                // Libératon du bloc
                brelse(bh2);

                lu += bh_size;
                // Passe au bloc suivant
                offset_block++;
                offset = 0;
        }
	
	// Copie vers l'utilisateur
        if(copy_to_user(user_buf, buf, lu)){
                brelse(bh);
                kfree(buf);
                return -1;
        }
	
        // Libératon du bloc
        brelse(bh);
        kfree(buf);

        return lu;
}

ssize_t ouichefs_write(struct file* file, const char __user* user_buf, size_t size, loff_t* ppos){
                
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
        size_t written_data = 0, len = 0, premier = 0;
        
        // Recuperer le tableau des blocs
        bh = sb_bread(sb, ci->index_block);
        if (!bh){
                return -1;
        }
        
        file_block = (struct ouichefs_file_index_block *)bh->b_data;

        // Numéro de bloc correspondant à l'offset
        sector_t offset_block = (*ppos >> sb->s_blocksize_bits);

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
                                                
                        file_block->blocks[offset_block] = num_block;
                        // Mis a jour de l'inode
                	vfs_inode->i_blocks++;
                        premier = 1;
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

        bh2 = sb_bread(sb, nb_block);
        if (!bh2){
                return -1;
        }

        data = bh2->b_data + offset;
        // Buffer qui contient les donnees après l'offset
        size_t size_from_offset = sb->s_blocksize - offset;
        buf = kmalloc(size_from_offset, GFP_KERNEL);
        memcpy(buf, data, size_from_offset);
        
        while(written_data < size){

                // Calcul len max à ecrire dans le bloc
                len = min(sb->s_blocksize - offset, size - written_data);
                
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

                        // Decalage de tous les blocs
                        for(int i = vfs_inode->i_blocks; i>offset_block; i--){
                                file_block->blocks[i] = file_block->blocks[i-1];
                        }
                        
                        offset_block++;
                        file_block->blocks[offset_block] = num_block;

                        // Mis a jour de l'inode
                        vfs_inode->i_blocks++;

                        written_data += size_new_block;
                        offset = size_new_block;
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
     			     			
     			// Ajout de padding entre size_used et offset
                	int size_used_to_offset =  offset - size_used;
     			int size_current_block = ((size_used + len) >= OUICHEFS_BLOCK_SIZE) && !premier ? OUICHEFS_BLOCK_SIZE : (size_used + len);
			if(size_used_to_offset > 0){
				size_current_block += size_used_to_offset;
			}
                	
                        // Calcul du numero de bloc
                        num_block = NB_BLOCK_WITH_SIZE(nb_block, size_current_block);
                        file_block->blocks[offset_block] = num_block;

                        written_data += len;
                        offset += len;
                }
		premier = 0;
	}
	
        // Ajout donnée du buffer
	
	size_used = file_block->blocks[offset_block];
        nb_block = file_block->blocks[offset_block];
        // Nombre de données réel dans le bloc
        size_used = TAILLE_BLOCK(size_used); 
        // Numero du bloc réel
        nb_block = NB_BLOCK(nb_block);
        
        if(sb->s_blocksize - size_used >= strlen(buf)){
        	
                bh3 = sb_bread(sb, nb_block);
                if (!bh3){
                        return -1;
                }
                memcpy(bh3->b_data + (*ppos % sb->s_blocksize) +len, buf, (size_t)strlen(buf));
                mark_buffer_dirty(bh3);
                sync_dirty_buffer(bh3);
                brelse(bh3);
                
        }
        else{
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
                }
                
                offset_block++;
                file_block->blocks[offset_block] = num_block;

                // Mis a jour de l'inode
                vfs_inode->i_blocks++;
		
        }
	
        brelse(bh);
        brelse(bh2);
        kfree(buf);
        
        // Mis à jour de la position dans le fichier
        file->f_pos += written_data;
        vfs_inode->i_size += written_data;
        
        return written_data;
}
