#define taille_block(nb) ((nb) >> 20 != 0 ? (nb) >> 20 : OUICHEFS_BLOCK_SIZE) 
#define nb_block(nb) ((nb) & 0xFFFFF)
#define nb_block_with_size(nb,size) (((size) << 20) || ((nb) & 0xFFFFF))

long ouichefs_defrag(struct file* file, unsigned int cmd, unsigned long arg){
    pr_info("OUICHEFS DEFRAG\n");

    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    struct inode *vfs_inode = &(ci->vfs_inode);
    struct buffer_head *bh_arriere = NULL;
    struct buffer_head *bh_avant = NULL;
    struct ouichefs_file_index_block *file_block = NULL;

    uint32_t used_size_arriere, nb_block_arriere;
    uint32_t used_size_avant, nb_block_avant;
    uint32_t dispo_arriere;

    bh_arriere = sb_bread(sb, ci->index_block);
    if (!bh_arriere){
            return -EIO;
    }
    file_block = (struct ouichefs_file_index_block *)(bh_arriere->b_data);

    int i_avant, i_arriere;
    for(i_avant = 0; i_arriere = 0; i_arriere < inode->i_blocks && i_avant < inode->i_blocks;){
        
        // aller chercher le prochain block non rempli
        while(i_arriere < inode->i_blocks && file_block->blocks[i_arriere]!=0)
            i_arriere++;

        if(i_arriere==inode->i_blocks)
            break;

        i_avant= i_avant == 0 ? i_arriere+1 : i_avant;

        // aller chercher le prochain block non vide
        while(i_avant < inode->i_blocks && file_block->blocks[i_avant]==0)
            i_avant++;

        if(i_avant==inode->i_blocks)
            break;


        // recupérer les infos sur les blocks et les blocks eux mêmes
        used_size_arriere =  taille_block(file_block->blocks[i_arriere]);
        used_size_avant = taille_block(file_block->blocks[i_avant]);
        nb_block_arriere = nb_block(file_block->blocks[i_arriere]);
        nb_block_avant = nb_block(file_block->blocks[i_avant]);


        bh_arriere = sb_bread(sb, file_block->blocks[i_arriere]);
        bh_avant = sb_bread(sb, file_block->blocks[i_avant]);
        
        dispo_arriere = OUICHEFS_BLOCK_SIZE - used_size_arriere; 

        // cas où le block arrière peut accueillir toutes les données du block avant
        if(dispo_arriere >= used_size_avant){
            for(int i = 0; i< used_size_avant; i++){
                bh_arriere->b_data[used_size_arriere + i] = bh_avant->b_data[i];
            }  
            mark_buffer_dirty(bh_arriere);
            put_block(nb_block_avant);
            file_block->blocks[i_avant] = 0;
            i_avant++;

        // cas où le block arrière ne peut pas accueillir toutes les données du block avant
        }else{
            for(int i = 0; i< dispo_arriere; i++){
                bh_arriere->b_data[used_size_arriere + i] = bh_avant->b_data[i];
            }
            for(int i = dispo_arriere; i<used_size_avant; i++){
                bh_avant->b_data[i] = bh_avant->b_data[dispo_arriere + i];
            }
            file_block->blocks[i_arriere] = nb_block_with_size(nb_block_arriere,used_size_arriere+dispo_arriere);
            file_block->blocks[i_avant] = nb_block_with_size(nb_block_avant,used_size_avant-dispo_arriere);
            mark_buffer_dirty(bh_arriere);
            mark_buffer_dirty(bh_avant);
            i_arriere++;
        }
        brelse(bh_arriere);
        brelse(bh_avant);
        mark_inode_dirty(vfs_inode);
    }
    return 0;
}