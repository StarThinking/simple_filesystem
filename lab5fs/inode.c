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
    struct lab5fs_ino *mem_ino,ri;
    struct buffer_head *bh;
    int block, off;
    
    printk("lab5fs_read_inode\n");
    printk("ino = %d\n", ino);
    
    if (ino < LAB5FS_ROOT_INO){
        printk("Bad inode number %s:%08lx\n", inode->i_sb->s_id, ino);
        make_bad_inode(inode);
        return;
    }
    
    block = 1 + 8 + 1 + 1 + ino/LAB5FS_INO_PER_BLOCK; 
    printk("block:%d\n",block);
    bh = sb_bread(inode->i_sb, block);
    if (!bh) {
        printk("Unable to read inode %s:%08lx\n", inode->i_sb->s_id, ino);
        make_bad_inode(inode);
        return;
    }
    printk("ino = %d, ino %% %d = %d %d\n", ino, LAB5FS_INO_PER_BLOCK, ino % LAB5FS_INO_PER_BLOCK, ino % 4);
    off = (ino % LAB5FS_INO_PER_BLOCK) * LAB5FS_INODESIZE;
    printk("off = %d\n", off);
    printk("total off = 0x%x\n", off+block*LAB5FS_BLOCKSIZE);
    mem_ino = (struct lab5fs_ino *)(bh->b_data + off);

    ri = *mem_ino;
    printk("i_ino: %d, i_type: %d, i_mode: %x, i_gid: %d, i_nlink: %d, i_blocknum: %d, i_atime: %d\n", ri.i_ino, ri.i_type, ri.i_mode, ri.i_gid, ri.i_nlink, ri.i_blocknum, ri.i_atime);
    
    inode->i_mode = 0x0000ffff & mem_ino->i_mode;
    if (mem_ino->i_type == LAB5FS_DIR_TYPE) {
        printk("LAB5FS_DIR_TYPE\n");
        inode->i_mode |= S_IFDIR;
        inode->i_op = &lab5fs_dir_inops;
        inode->i_fop = &lab5fs_dir_operations;
    } else if (mem_ino->i_type == LAB5FS_REG_TYPE) {
        printk("LAB5FS_REG_TYPE\n");
        inode->i_mode |= S_IFREG;
        //TODO
//        inode->i_op = &lab5fs_file_inops;
//        inode->i_fop = &lab5fs_file_operations;
//        inode->i_mapping->a_ops = &lab5fs_aops;
    }
    else {
        printk("FILE TYPE ERROR!\n");
        goto out;
    }
    
    
    inode->i_uid = mem_ino->i_uid;
    inode->i_gid = mem_ino->i_gid;
    inode->i_nlink = mem_ino->i_nlink;
    inode->i_blocks = LAB5FS_FILEBLOCKS(mem_ino);
    inode->i_size = LAB5FS_FILESIZE(mem_ino);
    inode->i_blksize = PAGE_SIZE;
    inode->i_atime.tv_sec = mem_ino->i_atime;
    inode->i_mtime.tv_sec = mem_ino->i_mtime;
    inode->i_ctime.tv_sec = mem_ino->i_ctime;
    inode->i_atime.tv_nsec = 0;
    inode->i_mtime.tv_nsec = 0;
    inode->i_ctime.tv_nsec = 0;

out:
    brelse(bh);

    printk("read_inode finished, PAGE_SIZE = %lu\n", PAGE_SIZE);
}


static struct super_operations lab5fs_sops = {
    .read_inode = lab5fs_read_inode,
};

static int lab5fs_fill_super(struct super_block *s, void *data, int silent){
    struct buffer_head *bh;
    struct lab5fs_sb *mem_sb;
    struct inode *inode;

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
    
    return 0; 

out:
    printk("something is wrong in fill_super\n");
    brelse(bh);
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
    printk(KERN_INFO "lab5fs_init()\n");
    
    err = register_filesystem(&lab5fs_fs_type);
    if (err){
        printk("register error\n");
        return err;
    }
    
    printk("register successfully\n");
    
    return 0;
}

void lab5fs_exit(void) {
    printk(KERN_INFO "lab5fs_exit()\n");
    
    unregister_filesystem(&lab5fs_fs_type);
}

MODULE_LICENSE("GPL");   
module_init(lab5fs_init);
module_exit(lab5fs_exit);
