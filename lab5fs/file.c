#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/smp_lock.h>

#include "lab5fs.h"

struct file_operations lab5fs_file_operations = {
    .llseek     = generic_file_llseek,
    .read       = generic_file_read,
    .write      = generic_file_write,
    .mmap       = generic_file_mmap,
    .sendfile   = generic_file_sendfile,
};

static int lab5fs_get_block(struct inode * inode, sector_t block, 
    struct buffer_head * bh_result, int create) {
    long phys;
    int block_data_start = 1 + 8 + 1 + 1 + 256 + 32; // 299
    struct super_block *sb = inode->i_sb;
    struct lab5fs_inode_info *bi = LAB5FS_I(inode);

    int _block, index;
    struct buffer_head * bh_data_map;
    struct lab5fs_datamap * data_map_p;
    
    printk("lab5fs_get_block(), block = %llu\n", block);
    
    if (block < 0) {
        printk("error: block < 0\n");
        return -EIO;
    }

    printk("bi->bi_blocks[%llu] = %u\n", block, bi->bi_blocks[block]);

    phys = bi->bi_blocks[block];
    if (!create) {
        if (phys > 0) { // check phys exits in bi->bi_blocks[]
            printk("\tc=%d, b=%llu, phys=%ld (granted)\n", create, block, phys);
            map_bh(bh_result, sb, phys);
        }
        printk("\n");
        return 0;
    }
    
    // if the file is not empty and the requested block is within the range
    //       of blocks allocated for this file, we can grant it
    printk("inode->i_size = %llu, phys = %ld, bi->bi_blocks[%llu] = %u\n",
        inode->i_size, phys,  block, bi->bi_blocks[block]);
    
    if (bi->bi_blocks[0] > 0 && phys > 0) {
        printk("\tc=%d, b=%llu, phys=%ld (interim block granted)\n", 
            create, block, phys);
        map_bh(bh_result, sb, phys);
        printk("\n");
        return 0;
    }
    
    lock_kernel();

    /* Ok, we have to move this entire file to the next free block */
    _block = 1;
    bh_data_map = sb_bread(inode->i_sb, _block);
    if (!bh_data_map) {
        printk("Unable to read data map\n");
    }
    data_map_p = (struct lab5fs_datamap *) (bh_data_map->b_data);
    index = find_first_zero_bit(data_map_p, LAB5FS_BLOCKSIZE*8*8);
    printk("the first zero bit index is %d\n", index);
    // flush data bitmap to disk
    set_bit(index,  data_map_p);
    mark_buffer_dirty(bh_data_map);
    brelse(bh_data_map);
    printk("flush data bitmap to disk\n");
    
    phys = block_data_start + index; // tmp: the first block is free
    printk("\tc=%d, b=%llu, phys=%ld (moved)\n", create, block, phys);
    
    bi->bi_blocks[block] = phys;
    //printk("bi->bi_blocks[%llu] = %u\n", block, bi->bi_blocks[block]);
    
    mark_inode_dirty(inode);
    map_bh(bh_result, sb, phys);
    
    unlock_kernel();
    printk("\n");
    return 0;
}

static int lab5fs_writepage(struct page *page, struct writeback_control *wbc) {
    printk("lab5fs_writepage()\n");
    printk("\n");
    return block_write_full_page(page, lab5fs_get_block, wbc);
}

static int lab5fs_readpage(struct file *file, struct page *page) {
    printk("lab5fs_readpage()\n");
    printk("\n");
    return block_read_full_page(page, lab5fs_get_block);
}

static int lab5fs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to) {
    printk("lab5fs_prepare_write()\n");
    printk("\n");
    return block_prepare_write(page, from, to, lab5fs_get_block);
}

struct address_space_operations lab5fs_aops = {
    .readpage   = lab5fs_readpage,
    .writepage  = lab5fs_writepage,
    .sync_page  = block_sync_page,
    .prepare_write  = lab5fs_prepare_write,
    .commit_write   = generic_commit_write,
};

struct inode_operations lab5fs_file_inops;
