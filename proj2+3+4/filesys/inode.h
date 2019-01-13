#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "../devices/block.h"
#include "off_t.h"
#include "../lib/kernel/list.h"


struct bitmap;


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define LEVEL1 123
#define FULLLEVEL 128
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
unsigned char zeros[BLOCK_SECTOR_SIZE];
struct inode_disk
{
    bool is_dir;               /* First data sector.  32 */
    off_t length;                       /* File size in bytes. 64 */
    uint32_t sector_level1[LEVEL1];
    uint32_t sector_level2;
    uint32_t sector_level3;
    unsigned magic;                     /* Magic number. */
//    uint32_t unused[125];               /* Not used. */
};

/* In-memory inode. */
struct inode
{
    struct list_elem elem;              /* Element in inode list. 64*/
    struct dir* father;
    block_sector_t sector;              /* Sector number of disk location. 32*/
    int open_cnt;                       /* Number of openers. 32*/
    bool removed;                       /* True if deleted, false otherwise. 32*/
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. 32*/
    struct inode_disk data;             /* Inode content. */
};

void inode_init (void);
bool inode_create (block_sector_t sector, off_t length,bool is_dir);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *inode, const void *buffer_, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */
