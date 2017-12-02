#include <linux/time.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/smp_lock.h>
#include <linux/buffer_head.h>
#include <linux/sched.h>

#include "lab5fs.h"

static int lab5fs_readdir(struct file * f, void * dirent, filldir_t filldir) {
    struct inode * dir = f->f_dentry->d_inode;
    int size;
    int block;
    struct buffer_head *bh_dentry_map, *bh;
    struct lab5fs_dentry *dentry_p;
    struct lab5fs_dentrymap *dentry_map_p;
    int set_bit_pos = -1;
    int dentry_map_size, dentry_index, dentry_block_index, dentry_within_block_index;
  
    lock_kernel();
    
    printk("lab5fs_readdir()\n");
    printk("init f->f_pos = %u\n", f->f_pos);
    printk("dir->ino = %u, dir->i_size = %u\n", dir->i_ino, dir->i_size);

    if (f->f_pos & (LAB5FS_DENTRYSIZE-1)) {
        printk("Bad f_pos=%08lx for %s:%08lx\n", (unsigned long)f->f_pos, 
                dir->i_sb->s_id, dir->i_ino);
        unlock_kernel();
        return -EBADF;
    }

    if (f->f_pos >= dir->i_size) {
        unlock_kernel();
        return 0;
    }
    
    /* Read dentry map */
    block = 1 + 8;
    bh_dentry_map = sb_bread(dir->i_sb, block);
    if (!bh_dentry_map) { 
        printk("Unable to read dentry map\n");
    }
    dentry_map_p = (struct lab5fs_dentrymap *) (bh_dentry_map->b_data);
    printk("Dentry map is %04x\n", *((unsigned int*)dentry_map_p));
    
    block = 1 + 8 + 1 + 1 + 256;
    dentry_map_size = LAB5FS_BLOCKSIZE*8;
    while ((set_bit_pos = find_next_bit((unsigned long *)dentry_map_p, dentry_map_size, set_bit_pos+1)) < dentry_map_size) {
        dentry_index = set_bit_pos;
        dentry_block_index = dentry_index / LAB5FS_BLOCKSIZE;
        dentry_within_block_index = dentry_index % LAB5FS_DENTRY_PER_BLOCK;
        
        printk("dentry_index = %d, dentry_block_index = %d, dentry_within_block_index = %d\n", 
                dentry_index, dentry_block_index, dentry_within_block_index);
        
        bh = sb_bread(dir->i_sb, block + dentry_block_index);
        if (!bh) { 
            printk("Unable to read dentry block\n");
        }
        dentry_p = (struct lab5fs_dentry *) (bh->b_data);
        dentry_p += dentry_within_block_index;
        printk("dentry: ino = %u, name = %s\n", dentry_p->ino, dentry_p->name);
        
        size = strnlen(dentry_p->name, LAB5FS_NAMELEN);
        if (filldir(dirent, dentry_p->name, size, f->f_pos, dentry_p->ino, DT_UNKNOWN) < 0) {
            printk("filldir error\n");
        }
        f->f_pos += LAB5FS_DENTRYSIZE;
       
        brelse(bh);
    }
    
    brelse(bh_dentry_map);
    
    unlock_kernel();
    return 0;
}

struct file_operations lab5fs_dir_operations = {
    .read       = generic_read_dir,
    .readdir    = lab5fs_readdir,
    .fsync      = file_fsync,
};

static struct dentry * lab5fs_lookup(struct inode * dir, struct dentry * dentry, struct nameidata *nd) { 
   /* *truct inode * inode = NULL;
    struct buffer_head * bh;
    struct lab5fs_dentry * de;
    */
    printk("lab5fs_lookup()\n");
      
    /*if (dentry->d_name.len > LAB5FS_NAMELEN)
        return ERR_PTR(-ENAMETOOLONG);
    else
        printk("dentry's name is %s\n", dentry->d_name.name);
    */
    return NULL;
}
    
struct inode_operations lab5fs_dir_inops = {
    .lookup         = lab5fs_lookup,
};
