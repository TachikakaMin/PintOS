//
// Created by YIMIN TANG on 12/27/18.
//
#include "devices/block.h"
#include <list.h>
#include <string.h>
#include <stdio.h>
#include "devices/ide.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/sector-buffer.h"
#include "sector-buffer.h"
#include "filesys.h"

void init_sector_buffer(){
    list_init(&sector_buffer_list);
    lock_init(&sector_buffer_lock);
}

struct sector_buffer * find_in_sector_buffer(block_sector_t sector){
    struct list_elem *e;
    for (e = list_begin (&sector_buffer_list); e != list_end (&sector_buffer_list);
         e = list_next (e))
    {
        struct sector_buffer *f = list_entry (e, struct sector_buffer, elem);
        if (f->sector_num == sector) {
            list_remove(e);
            list_push_front(&sector_buffer_list,e);
            return f;
        }
    }
    return NULL;
}


void write_back(struct sector_buffer *f){
    if (f->dirty) block_write(f->block_own, f->sector_num, f->data);
    f->dirty = false;
}

void flush_all_sector()
{
    struct list_elem *e;
    lock_acquire(&sector_buffer_lock);
    for (e = list_begin (&sector_buffer_list); e != list_end (&sector_buffer_list);
         e = list_next (e))
    {
        struct sector_buffer *f = list_entry (e, struct sector_buffer, elem);
        write_back(f);
    }
    lock_release(&sector_buffer_lock);
}


struct sector_buffer * evict(struct block *block, block_sector_t sector)
{
    if (list_size(&sector_buffer_list) >= CACHESIZE) {
        struct list_elem *e = list_end (&sector_buffer_list )->prev;
        struct sector_buffer *f = list_entry (e, struct sector_buffer, elem);
        write_back(f);
        f->sector_num = sector;
        f->dirty = false;
        f->block_own = block;
        block_read(block, sector, f->data);
        list_remove(e);
        list_push_front(&sector_buffer_list,e);
        return f;
    }
    else {
        struct sector_buffer *f = (struct sector_buffer *) malloc(sizeof(struct sector_buffer));
        f->dirty = false;
        f->sector_num = sector;
        f->block_own = block;
        block_read(block, sector, f->data);
        list_push_front(&sector_buffer_list,&f->elem);
        return f;
    }
}





void cache_buffer_read(struct block *block, block_sector_t sector, const void *buffer)
{
    lock_acquire(&sector_buffer_lock);
    struct sector_buffer * f = find_in_sector_buffer(sector);
    if (f==NULL) f = evict(block, sector);
    memcpy (buffer, f->data, BLOCK_SECTOR_SIZE);
    lock_release(&sector_buffer_lock);
}


void cache_buffer_write(struct block *block, block_sector_t sector, const void *buffer)
{
    lock_acquire(&sector_buffer_lock);
    struct sector_buffer * f = find_in_sector_buffer(sector);
    if (f==NULL) f = evict(block, sector);
    f->dirty = true;
    memcpy (f->data, buffer, BLOCK_SECTOR_SIZE);
    lock_release(&sector_buffer_lock);
}