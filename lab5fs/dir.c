#include <linux/time.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/smp_lock.h>
#include <linux/buffer_head.h>
#include <linux/sched.h>

#include "lab5fs.h"

static int lab5fs_add_entry(struct inode * dir, const char * name, int namelen, int ino);
static struct buffer_head * lab5fs_find_entry(struct inode * dir, 
        const char * name, int namelen, struct lab5fs_dentry ** res_dir);

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
    printk("init f->f_pos = %llu\n", f->f_pos);
    printk("dir->ino = %lu, dir->i_size = %llu\n", dir->i_ino, dir->i_size);

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
    
    // read dentry map 
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

static int lab5fs_create(struct inode * dir, struct dentry * dentry, int mode, struct nameidata *nd) {
    int err;
    struct inode * inode;
    struct super_block * s = dir->i_sb;
    unsigned long ino;
    int block;
    struct buffer_head *bh_inode_map;
    struct lab5fs_inomap *inode_map_p;
    
    /*int ino_block_index, ino_within_block_index;
    struct buffer_head *bh_inode_block;
    struct lab5fs_ino *_inode_p;
    */
    
    printk("lab5fs_create(), name = %s, len = %d\n", dentry->d_name.name, dentry->d_name.len);
    
    inode = new_inode(s);
    if (!inode)
        return -ENOSPC;
    lock_kernel();
    
    // read inode map from disk
    block = 1 + 8 + 1;
    bh_inode_map = sb_bread(dir->i_sb, block);
    if (!bh_inode_map) { 
        printk("Unable to read inode map\n");
    }
    inode_map_p = (struct lab5fs_inomap *) (bh_inode_map->b_data);
    printk("Inode map is %04x\n", *((unsigned int*)inode_map_p));
    ino = find_first_zero_bit((unsigned long *)inode_map_p, LAB5FS_BLOCKSIZE*8);  
    printk("first zero bit index (new ino) = %lu\n", ino);

    if (ino > LAB5FS_BLOCKSIZE*8) {
        unlock_kernel();
        printk("error: ino is beyond ino map limit\n");
        iput(inode);
        return -ENOSPC;
    }
    
    set_bit(ino, (unsigned long *)inode_map_p); 
    // flush inodemap to disk
    mark_buffer_dirty(bh_inode_map);
    brelse(bh_inode_map);
    printk("flush inodemap to disk\n");

    inode->i_uid = current->fsuid;
    inode->i_gid = (dir->i_mode & S_ISGID) ? dir->i_gid : current->fsgid;
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    inode->i_blocks = inode->i_blksize = 0;
    inode->i_op = &lab5fs_file_inops;
    inode->i_fop = &lab5fs_file_operations;
    inode->i_mapping->a_ops = &lab5fs_aops;
    inode->i_mode = mode;
    inode->i_ino = ino;
    LAB5FS_I(inode)->i_dsk_ino = ino;
    LAB5FS_I(inode)->i_endoffset = 0;
    LAB5FS_I(inode)->i_blocknum = 0;
    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    
    // read inode from disk
    /*block = 1 + 8 + 1 + 1;
    ino_block_index = ino / LAB5FS_INO_PER_BLOCK;
    ino_within_block_index = ino % LAB5FS_INO_PER_BLOCK;
    printk("ino_block_index = %d, ino_within_block_index = %d\n",
            ino_block_index, ino_within_block_index);
    
    bh_inode_block = sb_bread(dir->i_sb, block + ino_block_index);
    if (!bh_inode_block) {
        printk("Unable to read inode block\n");
    }

    _inode_p = (struct lab5fs_ino *) (bh_inode_block->b_data);
    _inode_p += ino_within_block_index;
     
    _inode_p->i_endoffset = 0; 
    _inode_p->i_blocknum = 0;
    
    // flush inode to disk
    mark_buffer_dirty(bh_inode_block);
    brelse(bh_inode_block);
    printk("flush inode to disk\n");
*/
    err = lab5fs_add_entry(dir, dentry->d_name.name, dentry->d_name.len, inode->i_ino);
    if (err) {
        printk("error when lab5fs_add_entry\n");
        inode->i_nlink--;
        mark_inode_dirty(inode);
        iput(inode);
        unlock_kernel();
        return err;
    }
    
    unlock_kernel();
    d_instantiate(dentry, inode);
    return 0;
}

static struct dentry * lab5fs_lookup(struct inode * dir, struct dentry * dentry, struct nameidata *nd) { 
    struct inode * inode = NULL;
    struct buffer_head * bh;
    struct lab5fs_dentry * de;
    
    printk("lab5fs_lookup(), dentry's name is %s\n", dentry->d_name.name);
    
    if (dentry->d_name.len > LAB5FS_NAMELEN)
        return ERR_PTR(-ENAMETOOLONG);
    
    lock_kernel();
    bh = lab5fs_find_entry(dir, dentry->d_name.name, dentry->d_name.len, &de);

    if (bh) {
        unsigned long ino = le32_to_cpu(de->ino);
        printk("dentry found, ino = %lu\n", ino);
        brelse(bh);
        inode = iget(dir->i_sb, ino);
        if (!inode) {
            printk("error: return -EACCES\n");
            unlock_kernel();
            return ERR_PTR(-EACCES);
        }
    }
    
    printk("dentry not found\n");
    unlock_kernel();
    d_add(dentry, inode);
    
    return NULL;
}
    
struct inode_operations lab5fs_dir_inops = {
    .create         = lab5fs_create,
    .lookup         = lab5fs_lookup,
};

static int lab5fs_add_entry(struct inode * dir, const char * name, int namelen, int ino) {
    int block;
    struct buffer_head *bh_dentry_map, *bh;
    struct lab5fs_dentrymap *dentry_map_p;
    struct lab5fs_dentry *dentry_p;
    int dentry_index, dentry_block_index, dentry_within_block_index;
    int i;
    
    printk("lab5fs_add_entry()\n");
    
    // read dentry bitmap
    block = 1 + 8;
    bh_dentry_map = sb_bread(dir->i_sb, block);
    if (!bh_dentry_map) { 
        printk("Unable to read dentry map\n");
        return -ENOSPC;
    }
    dentry_map_p = (struct lab5fs_dentrymap *) (bh_dentry_map->b_data);
    printk("Dentry map is %04x\n", *((unsigned int*)dentry_map_p));
    
    dentry_index = find_first_zero_bit((unsigned long *)dentry_map_p, LAB5FS_BLOCKSIZE*8);  
    printk("first zero bit index of dentry bitmap = %d\n", dentry_index);
    // flush dentry bitmap to disk
    set_bit(dentry_index, (unsigned long *)dentry_map_p); 
    mark_buffer_dirty(bh_dentry_map);
    brelse(bh_dentry_map);
    printk("flush dentry bitmap to disk\n");
    
    // read dentry block
    block = 1 + 8 + 1 + 1 + 256;
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
    
    if (!dentry_p->ino) {
        dir->i_size += LAB5FS_DENTRYSIZE;
        LAB5FS_I(dir)->i_blocknum ++;
        printk("LAB5FS_I(dir)->i_blocknum = %u\n", LAB5FS_I(dir)->i_blocknum);
        dir->i_ctime = CURRENT_TIME;
        dir->i_mtime = CURRENT_TIME;
        mark_inode_dirty(dir);
        
        // update dentry
        dentry_p->ino = ino;
        for (i=0; i<LAB5FS_NAMELEN; i++)
            dentry_p->name[i] = (i < namelen) ? name[i] : 0;
        
        printk("New dentry to be flushed: ino = %u, name = %s\n", dentry_p->ino, dentry_p->name);
    
        // flush dentry tp disk
        mark_buffer_dirty(bh);
        brelse(bh);
        return 0;
    }
    brelse(bh);
    return -ENOSPC;
}

static inline int lab5fs_namecmp(int len, const char * name, const char * buffer) {
    int ret = 0;

    printk("lab5fs_namecmp(), len = %d, name = %s\n", len, name);
    if (len < LAB5FS_NAMELEN && buffer[len])
        return ret;

    // if equals, memcmp() returns 0
    ret = !memcmp(name, buffer, len);
    printk("ret = %d\n", ret);
    return ret;
}

static struct buffer_head * lab5fs_find_entry(struct inode * dir, 
        const char * name, int namelen, struct lab5fs_dentry** res_dir) {
    int block;
    struct buffer_head *bh_dentry_map, *bh;
    struct lab5fs_dentry *dentry_p;
    struct lab5fs_dentrymap *dentry_map_p;
    int set_bit_pos = -1;
    int dentry_map_size, dentry_index, dentry_block_index, dentry_within_block_index;
  
    printk("lab5fs_find_entry()\n");
    printk("dir->ino = %lu, dir->i_size = %llu\n", dir->i_ino, dir->i_size);

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
       
        if (dentry_p->ino && lab5fs_namecmp(namelen, name, dentry_p->name)) {
            *res_dir = dentry_p;
            return bh;
        }
        brelse(bh);
        bh = NULL;
    }
    
    brelse(bh_dentry_map);
    return NULL;
}


