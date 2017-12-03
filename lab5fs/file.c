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

struct address_space_operations lab5fs_aops = {};

struct inode_operations lab5fs_file_inops;
