#ifndef __FS_H__
#define __FS_H__

#include <common.h>

enum access_flag{
    DIR_EXIST,
    FILE_EXIST,
    FILE_READ,
    FILE_WRITE,
    FILE_APPEND
};

filesystem *devfs_create();

filesystem *procfs_create();

filesystem *blkfs_create();

void inode_free(inode* node);

void fslist_node_free(fslist* node);

#endif