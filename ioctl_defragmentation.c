long ouichefs_ioctl(struct file* file, unsigned int cmd, unsigned long arg){
	
	int fd = (int)arg;
	struct file *data = fget(fd);
	
	struct inode *inode = file_inode(data);
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct inode *vfs_inode = &(ci->vfs_inode);
	struct buffer_head *bh = NULL;
	struct buffer_head *bh_arriere = NULL;
	struct buffer_head *bh_avant = NULL;
	struct ouichefs_file_index_block *file_block = NULL;
	uint32_t size_used, nb_block, num_block;
	uint32_t used_size_arriere, nb_block_arriere;
	uint32_t used_size_avant, nb_block_avant;
	uint32_t dispo_arriere;
	
	switch(cmd){
		case TEST_CMD:
	
			int nb_used_blocks = 0;
			int nb_partially_blocks = 0;
			int nb_bytes_wasted = 0;
			int delayed_bytes_wasted = 0;
			
			bh = sb_bread(sb, ci->index_block);
			if (!bh){
				return -1;
			}
			file_block = (struct ouichefs_file_index_block *)(bh->b_data);
				
			for(int i = 0; i < vfs_inode->i_blocks; i++){
				
				if(file_block->blocks[i] == 0){
				        break;
				}
			
				size_used = file_block->blocks[i];
				nb_block = file_block->blocks[i];
				// Nombre de données réel dans le bloc
				size_used = TAILLE_BLOCK(size_used);
				nb_block = NB_BLOCK(nb_block);
						  
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

			for(int i = 0; i < vfs_inode->i_blocks; i++){
				
				if(!file_block->blocks[i]){
				        break;
				}
				
				size_used = file_block->blocks[i];
				nb_block = file_block->blocks[i];
				// Nombre de données réel dans le bloc
				size_used = TAILLE_BLOCK(size_used); 
				// Numero du bloc réel
				nb_block = NB_BLOCK(nb_block); 

				pr_info("Numero de bloc = %d\nTaille du bloc effectif = %d\n", nb_block, size_used);
			}

			brelse(bh);
			fput(data);
			
			break;
			
		case DEFRAG_CMD:
			
			bh_arriere = sb_bread(sb, ci->index_block);
			if (!bh_arriere){
			    return -1;
			}
			file_block = (struct ouichefs_file_index_block *)(bh_arriere->b_data);

			int i_avant, i_arriere;
			for(i_avant = 0, i_arriere = 0; i_arriere < vfs_inode->i_blocks-1;){
								
				used_size_arriere =  file_block->blocks[i_arriere];
				used_size_arriere =  TAILLE_BLOCK(used_size_arriere);
				
				// aller chercher le prochain block partiel
				while(i_arriere < vfs_inode->i_blocks && used_size_arriere == OUICHEFS_BLOCK_SIZE){
					i_arriere++;
					used_size_arriere =  file_block->blocks[i_arriere];
					used_size_arriere =  TAILLE_BLOCK(used_size_arriere);
				}
				
				// si le block partiel correspond au dernier
				if(i_arriere == vfs_inode->i_blocks - 1){
					break;
				}

				i_avant = i_arriere + 1;

				used_size_avant = file_block->blocks[i_avant];
				used_size_avant = TAILLE_BLOCK(used_size_avant);
				
				if(file_block->blocks[i_avant] == 0){
					break;
				}

				// recupérer les infos sur les blocks et les blocks eux mêmes
				used_size_arriere =  file_block->blocks[i_arriere];
				used_size_avant = file_block->blocks[i_avant];
				nb_block_arriere = file_block->blocks[i_arriere];
				nb_block_avant = file_block->blocks[i_avant];
				
				used_size_arriere =  TAILLE_BLOCK(used_size_arriere);
				used_size_avant = TAILLE_BLOCK(used_size_avant);
				nb_block_arriere = NB_BLOCK(nb_block_arriere);
				nb_block_avant = NB_BLOCK(nb_block_avant);

				bh_arriere = sb_bread(sb, nb_block_arriere);
				bh_avant = sb_bread(sb, nb_block_avant);

				dispo_arriere = OUICHEFS_BLOCK_SIZE - used_size_arriere; 
				
				// cas où le block arrière peut accueillir toutes les données du block avant
				if(dispo_arriere >= used_size_avant){
						
					memcpy(bh_arriere->b_data + used_size_arriere, bh_avant->b_data, used_size_avant);
					mark_buffer_dirty(bh_arriere);
					sync_dirty_buffer(bh_arriere);
					put_block(sbi, nb_block_avant);
					file_block->blocks[i_avant] = 0;
					
					// Calcul du numero de bloc
                			num_block = NB_BLOCK_WITH_SIZE(nb_block_arriere, (used_size_arriere + used_size_avant)); 
					file_block->blocks[i_arriere] = num_block;
					
					// Decalage de tous les blocs
					for(int i = vfs_inode->i_blocks; i > i_avant; i--){
						file_block->blocks[i-1] = file_block->blocks[i];
						
					}

				// cas où le block arrière ne peut pas accueillir toutes les données du block avant
				}else{
					
					memcpy(bh_arriere->b_data + used_size_arriere, bh_avant->b_data, dispo_arriere);
					memcpy(bh_avant->b_data, bh_avant->b_data + dispo_arriere, (used_size_avant - dispo_arriere));	
					memset(bh_avant + (used_size_avant - dispo_arriere), 0, OUICHEFS_BLOCK_SIZE - (used_size_avant - dispo_arriere));
					file_block->blocks[i_arriere] = NB_BLOCK_WITH_SIZE(nb_block_arriere, (used_size_arriere + dispo_arriere));
					file_block->blocks[i_avant] = NB_BLOCK_WITH_SIZE(nb_block_avant, (used_size_avant - dispo_arriere));
					mark_buffer_dirty(bh_arriere);
					mark_buffer_dirty(bh_avant);
					sync_dirty_buffer(bh_arriere);
                        		sync_dirty_buffer(bh_avant);                        
					i_arriere++;
				}
				brelse(bh_arriere);
				brelse(bh_avant);
				mark_inode_dirty(vfs_inode);
			}
       
			break;
		
		default:
		
			pr_info("Error CMD\n");
			return -1;
	}
	
	return 0;
}
