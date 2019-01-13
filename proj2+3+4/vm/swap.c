#include "swap.h"
#include "vm/swap.h"
#include "../lib/kernel/bitmap.h"
#include "swap.h"
#include "../threads/vaddr.h"
#include "../devices/block.h"
#include "../threads/synch.h"
#include "../lib/debug.h"

const size_t SECTORS_PER_PAGE=(PGSIZE / BLOCK_SECTOR_SIZE);

struct lock swap_lock;
struct bitmap *swap_map;
struct block *swap_block;

void swap_init (void)
{
    lock_init(&swap_lock);
    if (swap_block = block_get_role (BLOCK_SWAP)) {
        swap_map = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE);
        if (!swap_map) return;
        bitmap_set_all(swap_map,0);
    }
    return;
}

size_t swap_out (void *frame)
{
    lock_acquire(&swap_lock);
    size_t free_index = bitmap_scan_and_flip(swap_map, 0, 1, 0);
    lock_release(&swap_lock);
    for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
        block_write(swap_block, (block_sector_t) (free_index * SECTORS_PER_PAGE + i), (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);
    return free_index;
}

void swap_in (size_t used_index, void* frame)
{
    if (!swap_block || !swap_map) return;
    lock_acquire(&swap_lock);
    bitmap_flip(swap_map, used_index);
    lock_release(&swap_lock);
    for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
        block_read(swap_block, (block_sector_t) (used_index * SECTORS_PER_PAGE + i), (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);
}
