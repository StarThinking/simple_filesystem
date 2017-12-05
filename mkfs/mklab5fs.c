#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <asm/bitops.h>

#include "../lab5fs/lab5fs.h"

int main(int argc, char **argv){
    char *fsname, *device;
    int total_bytes,ino_nums,ino_bytes,total_blocks,ino_blocks,data_blocks,dentry_blocks,dentrymap_blocks,datamap_blocks,inomap_blocks;
    int fd;
    uint32_t first_block;
    struct lab5fs_sb sb;
    struct lab5fs_ino ri;
    struct lab5fs_dentry de;
    struct lab5fs_inomap inomap;
    struct lab5fs_datamap datamap;
    struct lab5fs_dentrymap dentrymap;
    time_t now;
    int c,i,len;
    struct stat buf;
    int _i;
    
    if (argc != 2) {
        printf("need TWO arguments!\n");
        return;
    }

    //fsname = "            "; //12 bytes
    device = argv[1];
    printf("device name: %s\n", device);
        
    fd = open(device,O_RDWR);
    if (fd < 0) {
        printf("OPEN ERROR!\n");
        return;
    } else {
        printf("OPEN successfully\n");
        
        ino_nums = LAB5FS_INO_NUM; //1024
        ino_bytes = ino_nums * sizeof(struct lab5fs_ino);
       
        fstat(fd, &buf);
        total_bytes = buf.st_size;
        if (total_bytes > LAB5FS_MAX_IMAGESIZE){
            printf("IMAGE TOO LARGE! max file system size: 16M!\n");
            return;
        }
        if (total_bytes < LAB5FS_MIN_IMAGESIZE){
            printf("IMAGE TOO SMALL! min file system size: 1M!\n");
            return;
        }
        
        total_blocks = total_bytes/LAB5FS_BLOCKSIZE;
        ino_blocks = ino_nums*LAB5FS_INODESIZE/LAB5FS_BLOCKSIZE;
        dentry_blocks = ino_nums*LAB5FS_DENTRYSIZE/LAB5FS_BLOCKSIZE;
        datamap_blocks = 8;
        inomap_blocks = 1;
        dentrymap_blocks = 1;
        data_blocks = total_blocks-1-datamap_blocks-inomap_blocks-dentrymap_blocks-ino_blocks-dentry_blocks;
        printf("total blocks: %d, datamap blocks: %d, dentrymap blocks: %d, inomap blocks: %d, inode blocks: %d, dentry blocks: %d, data blocks: %d\n",\
                total_blocks, datamap_blocks, dentrymap_blocks, inomap_blocks, ino_blocks, dentry_blocks, data_blocks);
        
        // super block info
        memset(&sb, 0,sizeof(sb));
        snprintf(sb.s_fsname,LAB5FS_NAMELEN,device);
        sb.s_magic = (LAB5FS_SUPER_MAGIC);
        sb.s_start = (total_blocks-data_blocks-dentry_blocks) * LAB5FS_BLOCKSIZE;
        sb.s_end = total_blocks * LAB5FS_BLOCKSIZE - 1;
        printf("s_magic: %lx, s_start: %d, s_end: %d, s_fsname: %s\n",\
                sb.s_magic, sb.s_start, sb.s_end, sb.s_fsname); 
        
        //maps info
        memset(&datamap, 0, sizeof(datamap));
        memset(&dentrymap, 0, sizeof(dentrymap));
        memset(&inomap, 0, sizeof(inomap));
        
        //dentrymap.map[0] |= 3;
        //inomap.map[0] |= 3;
        for (i=0; i<=1; i++){ 
            set_bit(i, dentrymap.map);
        }
        set_bit(0, inomap.map);
        set_bit(1, inomap.map);
        set_bit(2, inomap.map);
        printf("dentrymap[0]: %x, inomap[0]: %x\n", dentrymap.map[0], inomap.map[0]);
        
        //write to file
        if (write(fd, &sb, sizeof(sb)) != sizeof(sb)){
            printf("error writing superblock\n");
            return;
        }
        printf("write superblock successfully\n");
        
        //write maps
        if (write(fd, &datamap, sizeof(datamap)) != sizeof(datamap)){
            printf("error writing datamap\n");
            return;
        }
        printf("write datamap %d bytes successfully\n", sizeof(datamap));
        
        if (write(fd, &dentrymap, sizeof(dentrymap)) != sizeof(dentrymap)){
            printf("error writing dentrymap\n");
            return;
        }
        printf("write dentrymap %d bytes successfully\n", sizeof(dentrymap));
        
        if (write(fd, &inomap, sizeof(inomap)) != sizeof(inomap)){
            printf("error writing inomap\n");
            return;
        }
        printf("write inomap %d bytes successfully\n", sizeof(inomap));
        
        memset(&ri, 0, sizeof(ri));
        
        //write root inode
        if (write(fd, &ri, sizeof(ri)) != sizeof(ri)){
            printf("error writing root inode\n");
            return;
        }
        if (write(fd, &ri, sizeof(ri)) != sizeof(ri)){
            printf("error writing root inode\n");
            return;
        }
        // root inode info
        ri.di_ino = LAB5FS_ROOT_INO;
        //ri.i_first_block = 1 + ino_blocks;
        //ri.i_last_block  = ri.i_first_block + (ino_nums*sizeof(de)-1)/LAB5FS_BLOCKSIZE;
        //ri.di_endsize = 2*sizeof(de)-1;
        ri.di_endsize = 0;
        ri.di_type = LAB5FS_DIR_TYPE;
        ri.di_mode = S_IFDIR | 0755;
        //ri.i_uid = ri.i_gid = 1;
        ri.di_nlink = 2;
        ri.di_blocknum = 2;
        ri.di_uid = 0;
        ri.di_gid = 0;
        time(&now);
        //memset(ri.di_blocks, 0, sizeof(LAB5FS_DATABLOCK_PER_INO));
        for (_i=0; _i<LAB5FS_DENTRYSIZE; _i ++) {
        }
        ri.di_atime = ri.di_mtime = ri.di_ctime = now;
        //printf("i_ino: %d, i_type: %d, i_mode: %x, i_gid: %d, i_nlink: %d, i_blocknum: %d, i_atime: %d\n", ri.i_ino, ri.i_type, ri.i_mode, ri.i_gid, ri.i_nlink, ri.i_blocknum, ri.i_atime);
        
        if (write(fd, &ri, sizeof(ri)) != sizeof(ri)){
            printf("error writing root inode\n");
            return;
        }
        printf("write root inode successfully\n");
        
        //lseek to dentry blocks
        if (lseek(fd, (total_blocks - data_blocks - dentry_blocks) * LAB5FS_BLOCKSIZE, SEEK_SET) == -1){
            printf("seek error\n");
            return;
        }
        printf("lseek successfully\n");
        
        memset(&de, 0, sizeof(de));
        de.ino = LAB5FS_ROOT_INO;
        memcpy(de.name,".",1);
        if (write(fd, &de, sizeof(de)) != sizeof(de)){
            printf("error writing dentry \".\" \n");
            return;
        }
        printf("write dentry: \".\" successfully\n");
        
        memcpy(de.name,"..",2);
        if (write(fd, &de, sizeof(de)) != sizeof(de)){
            printf("error writing dentry \"..\" \n");
            return;
        }
        printf("write dentry: \"..\" successfully\n");
            
        close(fd);
    } 
    
}
