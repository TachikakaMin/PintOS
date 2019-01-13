//
// Created by YIMIN TANG on 12/27/18.
//

#ifndef SRC_SECTOR_BUFFER_H
#define SRC_SECTOR_BUFFER_H
#include <stdbool.h>
#include "filesys/off_t.h"
#include "off_t.h"
#include <list.h>
#include <string.h>
#include <stdio.h>
#include "devices/ide.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "../threads/synch.h"
#include "../lib/debug.h"
#include "../lib/kernel/list.h"
#include "../lib/stdio.h"
#include "../threads/malloc.h"
#include "../devices/block.h"

#define CACHESIZE 64

struct sector_buffer{
    struct block * block_own;
    block_sector_t sector_num;
    bool dirty;
    struct list_elem elem;
    unsigned char data[BLOCK_SECTOR_SIZE];
};

struct list sector_buffer_list;
struct lock sector_buffer_lock;

void cache_buffer_write(struct block *block, block_sector_t sector, const void *buffer);
void cache_buffer_read(struct block *block, block_sector_t sector, const void *buffer);
void flush_all_sector();
void init_sector_buffer();
#endif //SRC_SECTOR_BUFFER_H
