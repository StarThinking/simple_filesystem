#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/buffer_head.h>
#include <asm/bitops.h>

#include "lab5fs.h"
 
static void lab5fs_read_inode(struct inode * inode) {
    int ino = (int)inode->i_ino;
    struct lab5fs_ino *di, ri;
    struct buffer_head *bh;
    int block, off;
    int i = 0;
    
    printk("lab5fs_read_inode(), ino = %d\n", ino);
    
    if (ino < LAB5FS_ROOT_INO){
        printk("Bad inode number\n");
        make_bad_inode(inode);
        return;
    }
    
    block = 1 + 8 + 1 + 1 + ino/LAB5FS_INO_PER_BLOCK; 
    bh = sb_bread(inode->i_sb, block);
    if (!bh) {
        printk("Unable to read inode %d\n", ino);
        make_bad_inode(inode);
        return;
    }
    //printk("ino = %d, ino %% %d = %d %d\n", ino, LAB5FS_INO_PER_BLOCK, ino % LAB5FS_INO_PER_BLOCK, ino % 4);
    off = (ino % LAB5FS_INO_PER_BLOCK) * LAB5FS_INODESIZE;
    //printk("off = %d\n", off);
    //printk("total off = 0x%x\n", off+block*LAB5FS_BLOCKSIZE);
    di = (struct lab5fs_ino *)(bh->b_data + off);

    ri = *di;
    //printk("i_ino: %d, i_type: %d, i_mode: %x, i_gid: %d, i_nlink: %d, i_blocknum: %d, i_atime: %d\n", ri.i_ino, ri.i_type, ri.i_mode, ri.i_gid, ri.i_nlink, ri.i_blocknum, ri.i_atime);
    
    inode->i_mode = 0x0000ffff & di->di_mode;
    if (di->di_type == LAB5FS_DIR_TYPE) {
        printk("LAB5FS_DIR_TYPE\n");
        inode->i_mode |= S_IFDIR;
        inode->i_op = &lab5fs_dir_inops;
        inode->i_fop = &lab5fs_dir_operations;
    } else if (di->di_type == LAB5FS_REG_TYPE) {
        printk("LAB5FS_REG_TYPE\n");
        inode->i_mode |= S_IFREG;
        inode->i_op = &lab5fs_file_inops;
        inode->i_fop = &lab5fs_file_operations;
        inode->i_mapping->a_ops = &lab5fs_aops;
    }
    else {
        printk("FILE TYPE ERROR!\n");
        goto out;
    }
    
    inode->i_uid = di->di_uid;
    inode->i_gid = di->di_gid;
    inode->i_nlink = di->di_nlink;
    //inode->i_blocks = LAB5FS_FILEBLOCKS(di);
    if (ino != LAB5FS_ROOT_INO)
        inode->i_size = LAB5FS_FILESIZE(di);
    else
        inode->i_size = 4096; // for . 
    inode->i_blksize = PAGE_SIZE;
    inode->i_atime.tv_sec = di->di_atime;
    inode->i_mtime.tv_sec = di->di_mtime;
    inode->i_ctime.tv_sec = di->di_ctime;
    inode->i_atime.tv_nsec = 0;
    inode->i_mtime.tv_nsec = 0;
    inode->i_ctime.tv_nsec = 0;
    LAB5FS_I(inode)->bi_dsk_ino = di->di_ino;
    memcpy(LAB5FS_I(inode)->bi_blocks, di->di_blocks, sizeof(uint32_t)*LAB5FS_DATABLOCK_PER_INO);
    inode->i_blocks = di->di_blocknum;
    
    mark_inode_dirty(inode);
    printk("after read: ino = %d, inode->i_size = %lld\n", ino, inode->i_size);
    printk("inode->i_blocks = %lu, inode->i_size = %llu\n", inode->i_blocks, inode->i_size);
   
    for(i=0; i<=15; i++) {
        printk("LAB5FS_I(inode)->bi_blocks[%d] = %u ", i, LAB5FS_I(inode)->bi_blocks[i]);
        printk("\n");
    }
out:
    brelse(bh);
    printk("\n");
}

static int lab5fs_write_inode(struct inode * inode, int unused) {
    int ino_block_index, ino_within_block_index;
    struct buffer_head *bh_inode_block;
    struct lab5fs_ino *di;
    int block;
    unsigned long ino = inode->i_ino;

    printk("lab5fs_write_inode(), ino = %lu\n", ino);
    
    lock_kernel();
    
    // read inode from disk
    block = 1 + 8 + 1 + 1;
    ino_block_index = ino / LAB5FS_INO_PER_BLOCK;
    ino_within_block_index = ino % LAB5FS_INO_PER_BLOCK;
    printk("ino_block_index = %d, ino_within_block_index = %d\n",
        ino_block_index, ino_within_block_index);

    bh_inode_block = sb_bread(inode->i_sb, block + ino_block_index);
    if (!bh_inode_block) {
        printk("Unable to read inode block\n");
        unlock_kernel();
        return -EIO;
    }

    di = (struct lab5fs_ino *) (bh_inode_block->b_data);
    di += ino_within_block_index;

    if (inode->i_ino == LAB5FS_ROOT_INO)
        di->di_type = LAB5FS_DIR_TYPE;
    else
        di->di_type = LAB5FS_REG_TYPE;
    
    di->di_ino = inode->i_ino;
    di->di_mode = inode->i_mode;
    di->di_uid = inode->i_uid;
    di->di_gid = inode->i_gid;
    di->di_nlink = inode->i_nlink;
    di->di_atime = inode->i_atime.tv_sec;
    di->di_mtime = inode->i_mtime.tv_sec;
    di->di_ctime = inode->i_ctime.tv_sec;
    
    if (di->di_ino != LAB5FS_ROOT_INO) {
        printk("inode->i_size = %llu\n", inode->i_size);
        di->di_endsize = (inode->i_size == 0) ? 0 : (inode->i_size % LAB5FS_BLOCKSIZE);
        di->di_blocknum = (inode->i_size == 0) ? 0 : (inode->i_size / LAB5FS_BLOCKSIZE) + 1;
    } else {
        di->di_endsize = 0; // not matter
        di->di_blocknum = inode->i_blocks;
    }

    memcpy(di->di_blocks, LAB5FS_I(inode)->bi_blocks, sizeof(uint32_t)*LAB5FS_DATABLOCK_PER_INO);

    printk("di->di_blocknum = %u, di->di_endsize = %u\n",
        di->di_blocknum, di->di_endsize);
    // flush inode to disk
    mark_buffer_dirty(bh_inode_block);
    brelse(bh_inode_block);
    printk("flush inode to disk\n");
    unlock_kernel();
    printk("\n");
    return 0;
}

static void lab5fs_delete_inode(struct inode * inode) {
    unsigned long ino = inode->i_ino;
    struct buffer_head *bh_inode_block;
    struct lab5fs_ino * di;
    int block;
    int ino_block_index, ino_within_block_index;
    
    struct buffer_head * bh_data_map, *bh_data_block;
    struct lab5fs_datamap * data_map_p;
    int i;
    int block_data_start = 1 + 8 + 1 + 1 + 256 + 32; // 299
    void *data_block;
    
    printk("lab5fs_delete_inode(), ino=%lu\n", ino);
    
    if (ino < LAB5FS_ROOT_INO){
        printk("Bad inode number\n");
        return;
    }
    
    // clear data and data bitmap
    block = 1;
    bh_data_map = sb_bread(inode->i_sb, block);
    if (!bh_data_map) {
        printk("Unable to read data map\n");
    }
    data_map_p = (struct lab5fs_datamap *) (bh_data_map->b_data);
    
    for (i=0; i<LAB5FS_DATABLOCK_PER_INO; i++) {
        //printk("LAB5FS_I(inode)->bi_blocks[%d] = %d\n", i, (int)(LAB5FS_I(inode)->bi_blocks[i]));
        if (LAB5FS_I(inode)->bi_blocks[i] > 0) {
            printk("LAB5FS_I(inode)->bi_blocks[%d] = %d\n", i, (int)(LAB5FS_I(inode)->bi_blocks[i]));
            int index = (int)(LAB5FS_I(inode)->bi_blocks[i]) - block_data_start;
            printk("clear data bitmap at %d bit\n", index);
            clear_bit(index, data_map_p);
            mark_buffer_dirty(bh_data_map);

            bh_data_block = sb_bread(inode->i_sb, block_data_start + index);
            data_block = (void *) bh_data_block->b_data;
            memset(data_block, 0, LAB5FS_BLOCKSIZE);
            printk("clear data block %d\n", index + block_data_start);
            mark_buffer_dirty(bh_data_block);
            brelse(bh_data_block);
        }
    }
    brelse(bh_data_map);
    
    inode->i_size = 0;
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    lock_kernel();
    mark_inode_dirty(inode);
    
    // read inode from disk
    block = 1 + 8 + 1 + 1;
    ino_block_index = ino / LAB5FS_INO_PER_BLOCK;
    ino_within_block_index = ino % LAB5FS_INO_PER_BLOCK;
    printk("ino_block_index = %d, ino_within_block_index = %d\n",
        ino_block_index, ino_within_block_index);

    bh_inode_block = sb_bread(inode->i_sb, block + ino_block_index);
    if (!bh_inode_block) {
        printk("Unable to read inode block\n");
        unlock_kernel();
    }
   
    // clear and flush to disk
    di = (struct lab5fs_ino *) (bh_inode_block->b_data);
    di += ino_within_block_index;
    memset(di, 0, sizeof(struct lab5fs_ino));
    mark_buffer_dirty(bh_inode_block);
    printk("clear inode and flush to disk\n");
    brelse(bh_inode_block);
    
    unlock_kernel();
    clear_inode(inode);

    printk("\n");
}

static void lab5fs_put_super(struct super_block *s) {
    printk("lab5fs_put_super()\n");
}
        
static kmem_cache_t * lab5fs_inode_cachep;

static struct inode *lab5fs_alloc_inode(struct super_block *sb) {
    struct lab5fs_inode_info *bi;
    
    printk("lab5fs_alloc_inode()\n");
    bi = kmem_cache_alloc(lab5fs_inode_cachep, SLAB_KERNEL);
    memset(bi->bi_blocks, 0, sizeof(uint32_t)*LAB5FS_DATABLOCK_PER_INO);
    //bi->i_blocknum = 0;
    //bi->i_endoffset = 0;
    printk("\n");
    if (!bi)
        return NULL;
    return &bi->vfs_inode;
}

static void lab5fs_destroy_inode(struct inode *inode) {
    kmem_cache_free(lab5fs_inode_cachep, LAB5FS_I(inode));
}

static void init_once(void * foo, kmem_cache_t * cachep, unsigned long flags) {
    struct lab5fs_inode_info *bi = foo;
    
    if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
            SLAB_CTOR_CONSTRUCTOR)
        inode_init_once(&bi->vfs_inode);
}

static int init_inodecache(void) {
    printk("init_inodecache()\n");
    lab5fs_inode_cachep = kmem_cache_create("lab5fs_inode_cache",
                            sizeof(struct lab5fs_inode_info),
                            0, SLAB_RECLAIM_ACCOUNT,
                            init_once, NULL);
    if (lab5fs_inode_cachep == NULL)
        return -ENOMEM;
    return 0;
}

static void destroy_inodecache(void) {
    printk("destroy_inodecache()\n");
    if (kmem_cache_destroy(lab5fs_inode_cachep))
        printk(KERN_INFO "lab5fs_inode_cache: not all structures were freed\n");
    printk("\n");
}

static struct super_operations lab5fs_sops = {
    .alloc_inode    = lab5fs_alloc_inode,
    .destroy_inode  = lab5fs_destroy_inode,
    .read_inode     = lab5fs_read_inode,
    .write_inode    = lab5fs_write_inode,
    .delete_inode   = lab5fs_delete_inode,
    .put_super      = lab5fs_put_super,
};

static int lab5fs_fill_super(struct super_block *s, void *data, int silent){
    struct buffer_head *bh;
    struct lab5fs_sb *mem_sb;
    struct inode *inode;

    printk("lab5fs_fill_super()\n");
    sb_set_blocksize(s, LAB5FS_BLOCKSIZE);

    bh = sb_bread(s, 0);
    if (!bh)
        goto out;
    mem_sb = (struct lab5fs_sb *)bh->b_data;
    if (mem_sb->s_magic != LAB5FS_MAGIC){
        if (!silent)
            printk("No LAB5FS filesystem on %s (magic=%08x)\n", s->s_id, mem_sb->s_magic);
        goto out;
    }

    s->s_magic = LAB5FS_MAGIC;
    printk("set magic successfully\n");
    
    // read_inode op
    s->s_op = &lab5fs_sops;
    
    // create root inode
    inode = iget(s, LAB5FS_ROOT_INO);
    if (!inode){
        printk("iget error\n");
        goto out;
    }
    s->s_root = d_alloc_root(inode);
    if (!s->s_root){
        printk("d_alloc_root error\n");
        iput(inode);
        goto out;
    }
    printk("d_alloc_root finished\n");
    printk("\n");
    
    return 0; 

out:
    printk("something is wrong in fill_super\n");
    brelse(bh);
    printk("\n");
    return -EINVAL;
}

static struct super_block *lab5fs_get_sb(struct file_system_type *fs_type,int flags, const char *dev_name, void *data) {
    return get_sb_bdev(fs_type, flags, dev_name, data, lab5fs_fill_super);
}

static struct file_system_type lab5fs_fs_type = {
    .owner = THIS_MODULE,
    .name = "lab5fs",
    .get_sb = lab5fs_get_sb,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

int lab5fs_init (void) {
    int err;
    
    printk("lab5fs_init()\n");
    init_inodecache();
    
    err = register_filesystem(&lab5fs_fs_type);
    if (err) {
        printk("register error\n");
        return err;
    }
    printk("register successfully\n");
    return 0;
}

void lab5fs_exit(void) {
    printk(KERN_INFO "lab5fs_exit()\n");
    unregister_filesystem(&lab5fs_fs_type);
    destroy_inodecache();
}

MODULE_LICENSE("GPL");   
module_init(lab5fs_init);
module_exit(lab5fs_exit);
