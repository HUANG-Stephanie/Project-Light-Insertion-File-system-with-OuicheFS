#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE_MAX 4096

long ouichefs_ioctl(struct file* file, unsigned int cmd, unsigned long arg){
        int nb_used_blocks = 0;
        int nb_partially_blocks = 0;
        int nb_bytes_wasted = 0;
        int delayed_bytes_wasted = 0;

        struct inode *inode = file_inode(file);
        struct super_block *sb = inode->i_sb;
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        struct buffer_head *bh = NULL;
        struct ouichefs_file_index_block *file_block = NULL;

        bh = sb_bread(sb, ci->index_block);
        if (!bh)
                return -EIO;
        file_block = (struct ouichefs_file_index_block *)bh->b_data;
        for(int i = 0; i < inode->i_blocks - 1; i++){
                
                if(!file_block->blocks[i]){
                        pr_info("Termine\n");
                        break;
                }

                // Décale les 12 bits de poids fort à droite
                uint32_t size_used = file_block->blocks[i] >> 20;
                // Masque qui conserve uniquement les 20 bits de poids faible
                uint32_t nb_block = file_block->blocks[i] & 0xFFFFF;
                
                nb_bytes_wasted += delayed_bytes_wasted;
                nb_used_blocks++;
                if(size_used != SIZE_MAX){
                        nb_partially_blocks++;
                        delayed_bytes_wasted += (SIZE_MAX - size_used);
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

                // Décale les 12 bits de poids fort à droite
                uint32_t size_used = file_block->blocks[i] >> 20;
                // Masque qui conserve uniquement les 20 bits de poids faible
                uint32_t nb_block = file_block->blocks[i] & 0xFFFFF;

                pr_info("Numero de bloc = %d\nTaille du bloc effectif = %d\n", nb_block, size_used);
        }

        brelse(bh);

        return 0;
}

static const struct file_operations ouichefs_fops = {
    .unlocked_ioctl = ouichefs_ioctl,
};