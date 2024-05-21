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
	        
        for(int i = 0; i < inode->i_blocks; i++){
    
            	uint32_t size_used = sb->s_blocksize - inode->i_size;
           
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
     
        fput(data);
	return 0;
}