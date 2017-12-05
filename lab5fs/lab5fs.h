#include <linux/list.h>

#define DEBUG

#ifdef DEBUG
#define printk(x...)   printk(x)
#else         
#define printk(x...)
#endif

#define LAB5FS_ROOT_INO         2
#define LAB5FS_NAMELEN          12
#define LAB5FS_BLOCKSIZE        512
#define LAB5FS_INODESIZE        128
#define LAB5FS_DENTRYSIZE       16
#define LAB5FS_SUPER_MAGIC      0x66badace
#define LAB5FS_MAGIC            LAB5FS_SUPER_MAGIC
#define LAB5FS_MAX_FILESIZE     8*1024  //8K
#define LAB5FS_INO_NUM          1024
#define LAB5FS_INODENUM          1024
#define LAB5FS_MAX_IMAGESIZE    16*1024*1024
#define LAB5FS_MIN_IMAGESIZE     1*1024*1024

#define LAB5FS_DATABLOCK_PER_INO    (LAB5FS_MAX_FILESIZE/LAB5FS_BLOCKSIZE)    //16
#define LAB5FS_INO_PER_BLOCK        (LAB5FS_BLOCKSIZE/LAB5FS_INODESIZE)   //4
#define LAB5FS_DENTRY_PER_BLOCK     (LAB5FS_BLOCKSIZE/LAB5FS_DENTRYSIZE)  //32

/*
//related to image size
#define LAB5FS_IMAGESIZE        8*1024*1024   //8M
#define LAB5FS_TOTALBLOCK_NUM   LAB5FS_IMAGESIZE/LAB5FS_BLOCKSIZE   //16384
#define LAB5FS_INOBLOCK_NUM     LAB5FS_INODENUM*LAB5FS_INODESIZE/LAB5FS_BLOCKSIZE   //256
#define LAB5FS_DATAMAPBLOCK_NUM LAB5FS_TOTALBLOCK_NUM/(LAB5FS_BLOCKSIZE*8)          //4
#define LAB5FS_INOMAPBLOCK_NUM  1
#define LAB5FS_DENTRYMAPBLOCK_NUM   1
#define LAB5FS_DENTRYBLOCK_NUM  LAB5FS_INODENUM*LAB5FS_DENTRYSIZE/LAB5FS_BLOCKSIZE  //32
#define LAB5FS_DATABLOCK_NUM    LAB5FS_TOTALBLOCK_NUM -1 -LAB5FS_DATAMAPBLOCK_NUM -LAB5FS_DENTRYMAPBLOCK_NUM -LAB5FS_INOMAPBLOCK_NUM -LAB5FS_INOBLOCK_NUM -LAB5FS_DENTRYBLOCK_NUM //16089
*/

#define LAB5FS_DIR_TYPE         2
#define LAB5FS_REG_TYPE         1

// super block - 512 bytes
struct lab5fs_sb {
    uint32_t s_magic;   //4 bytes, magic
    uint32_t s_start;   //4 bytes, byte offset of start of data
    uint32_t s_end;     //4 bytes, sizeof(slice)-1

    char s_fsname[LAB5FS_NAMELEN];
    char s_pad[488];    
};

// inode - 128 bytes
struct lab5fs_ino {
    uint32_t di_ino;
    //uint32_t i_unused;
    uint32_t di_endsize;   //offset of the last block
    uint32_t di_type, di_mode; //16 bytes so far

    uint32_t di_uid, di_gid, di_nlink;  
    uint32_t di_atime, di_mtime, di_ctime;   //40 bytes so far
    
    uint32_t di_blocknum;    //number of data blocks, max = 16
    uint32_t di_blocks[LAB5FS_DATABLOCK_PER_INO];  //64 bytes, data block numbers
    
    char i_pad[20];
};

// directory entry - 16 bytes
struct lab5fs_dentry {
    uint32_t ino;
    char name[LAB5FS_NAMELEN];
};

//data_map, ino_map
struct lab5fs_datamap{
    char map[8*LAB5FS_BLOCKSIZE]; //8 blocks
};

struct lab5fs_inomap{
    char map[LAB5FS_BLOCKSIZE]; //1 block
};

struct lab5fs_dentrymap{
    char map[LAB5FS_BLOCKSIZE]; //1 block
};

/*
 * lab5fs file system in-core inode info
 */
#ifdef __KERNEL__
struct lab5fs_inode_info {
    unsigned long bi_dsk_ino;
    //uint32_t i_endoffset;
    //uint32_t my_blocknum; // only used by file
    uint32_t bi_blocks[LAB5FS_DATABLOCK_PER_INO];
    struct inode vfs_inode;
};

static inline struct lab5fs_inode_info *LAB5FS_I(struct inode *inode) {
    return list_entry(inode, struct lab5fs_inode_info, vfs_inode);
}
#endif

#define D_SIZE(inode) (inode->i_blocks * LAB5FS_DENTRYSIZE)
#define LAB5FS_FILESIZE(di) (di->di_blocknum == 0? 0 : (di->di_blocknum-1)*LAB5FS_BLOCKSIZE + di->di_endsize)

/* file.c */
extern struct inode_operations lab5fs_file_inops;
extern struct file_operations lab5fs_file_operations;
extern struct address_space_operations lab5fs_aops;

/* dir.c */
extern struct inode_operations lab5fs_dir_inops;
extern struct file_operations lab5fs_dir_operations;
