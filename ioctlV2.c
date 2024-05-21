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
        //struct buffer_head *bh3 = NULL;
        struct ouichefs_file_index_block *file_block = NULL;
	uint32_t inode_block = (inode->i_ino / OUICHEFS_INODES_PER_BLOCK) + 1;
        uint32_t inode_shift = inode->i_ino % OUICHEFS_INODES_PER_BLOCK;
        	
        bh = sb_bread(sb, inode_block);
        if (!bh){
                return -EIO;
	}
	cinode = (struct ouichefs_inode *)(bh->b_data);
	cinode += inode_shift;
	
	pr_info("CI BLOC = %d\n", ci->index_block);	
	pr_info("CINODE BLOC = %d\n", cinode->index_block);	
	
        bh2 = sb_bread(sb, cinode->index_block);
        if (!bh2){
                return -EIO;
        }
        file_block = (struct ouichefs_file_index_block *)(bh2->b_data);
   
        pr_info("Premier bloc = %d\n", file_block->blocks[0]);
        pr_info("NB bloc inode = %llu\n", inode->i_blocks);
        pr_info("NB bloc cinode = %d\n", cinode->i_blocks);
	        
        for(int i = 0; i < inode->i_blocks; i++){
                pr_info("Premier bloc = %d\n", file_block->blocks[i]);
                pr_info("i = %d\n", i);
		                
                if(file_block->blocks[i] == 0){
                        pr_info("Termine\n");
                        break;
                }
	
        	// Nombre de données réel dans le bloc
                uint32_t size_used = file_block->blocks[i] >> 20;
                uint32_t nb_block = file_block->blocks[i] & 0xFFFFF;
                
                pr_info("BLOC REEL 2 = %d\n", nb_block);
                pr_info("TAILLE UTIL = %d\n", size_used);
                
                /*
                bh3 = sb_bread(sb, file_block->blocks[i]);
		if (!bh3){
		        return -EIO;
		}
		pr_info("DATA = %s\n",(char*)bh3->b_data);
 		brelse(bh3);
		*/
				  
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

        	// Nombre de données réel dans le bloc
                uint32_t size_used = file_block->blocks[i] >> 20;
        	// Numero du bloc réel
                uint32_t nb_block = file_block->blocks[i] & 0xFFFFF;

                pr_info("Numero de bloc = %d\nTaille du bloc effectif = %d\n", nb_block, size_used);
        }

        brelse(bh);
        brelse(bh2);        
        fput(data);
	return 0;
}
