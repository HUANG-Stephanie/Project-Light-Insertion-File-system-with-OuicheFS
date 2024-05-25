ssize_t ouichefs_read(struct file *file, char __user *user_buf, size_t size,
		      loff_t *ppos)
{
	pr_info("OUICHEFS READ\n");

	struct buffer_head *bh;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;

	// Numéro de bloc correspondant à l'offset
	sector_t nb_block = *ppos >> sb->s_blocksize_bits;

	// Lecture du bloc
	bh = sb_bread(sb, nb_block);
	if (!bh)
		return -1;

	// Calcul longueur max qu'on peut lire dans le bloc
	size_t bh_size = min(
		size, (size_t)(sb->s_blocksize - (*ppos % sb->s_blocksize)));

	// Copie vers l'utilisateur
	if (copy_to_user(user_buf, bh->b_data + (*ppos % sb->s_blocksize),
			 bh_size)) {
		brelse(bh);
		return -1;
	}
	// Mis à jour de la position dans le fichier
	file->f_pos += bh_size;

	// Libératon du bloc
	brelse(bh);
	return bh_size;
}

ssize_t ouichefs_write(struct file *file, const char __user *user_buf,
		       size_t size, loff_t *ppos)
{
	pr_info("OUICHEFS WRITE\n");

	char *data;
	size_t copy_bytes = 0;
	struct buffer_head *bh;
	struct inode *inode = file->f_inode;
	struct super_block *sb = inode->i_sb;

	// Numéro de bloc correspondant à l'offset
	sector_t nb_block = *ppos >> sb->s_blocksize_bits;

	// Position de l'offset dans le bloc
	unsigned int offset = *ppos % sb->s_blocksize;

	// Ecriture dans plusieurs blocs
	while (copy_bytes < size) {
		// Calcul len max à ecrire dans le bloc
		size_t len = min(sb->s_blocksize - offset, size - copy_bytes);

		bh = sb_bread(sb, nb_block);
		if (!bh)
			return -1;

		// Ecriture à la fin des données precédentes
		data = bh->b_data + offset;

		if (copy_from_user(data, user_buf + copy_bytes, len)) {
			brelse(bh);
			return -1;
		}

		// Marquer dirty et forcer l'écriture sur disque
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		brelse(bh);

		/* Incrémenter la longueur de donnée ecrite
		 *Incrémenter l'offset initial
		 *Passer au bloc suivant car fin du bloc courant
		 *offset à 0 car nouveau bloc
		 */
		copy_bytes += len;
		if (offset + len >= sb->s_blocksize) {
			nb_block++;
			inode->i_blocks++;
			offset = 0;
		}
	}
	inode->i_size += copy_bytes;
	file->f_pos += copy_bytes;
	return copy_bytes;
}
