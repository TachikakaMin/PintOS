#include <string.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "../threads/malloc.h"
#include "../lib/debug.h"
#include "../threads/thread.h"
#include "../userprog/process.h"
#include "../userprog/pagedir.h"
#include "frame.h"
#include "../lib/kernel/hash.h"
#include "page.h"
#include "../threads/vaddr.h"
#include "../userprog/process.h"
#include "../threads/palloc.h"
#include "swap.h"
#include "../threads/synch.h"
#include "../userprog/syscall.h"
#include "../filesys/file.h"
#include "../threads/loader.h"
#include "../threads/interrupt.h"
#include "../lib/stdio.h"

static unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
    struct page_table_entry *pte = hash_entry(e, struct page_table_entry, elem);
    return hash_int((int) pte->upage);
}

static bool page_hash_less (const struct hash_elem *x, const struct hash_elem *y, void *aux UNUSED)
{
    struct page_table_entry *a = hash_entry(x, struct page_table_entry, elem);
    struct page_table_entry *b = hash_entry(y, struct page_table_entry, elem);
    return a->upage < b ->upage;
}



void init_page_table(struct thread* t){
    hash_init(&t->pt,page_hash_func,page_hash_less,NULL);
}


struct page_table_entry* add_to_page_table(struct file *file, off_t ofs, uint8_t *upage,
                       uint32_t read_bytes, uint32_t zero_bytes, bool writable){
    struct page_table_entry* pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry));
    if (pte == NULL) return pte;
    pte->file = file;
    pte->ofs = ofs;
    pte->type = FILE;
    pte->loaded = false;
    pte->read_bytes = read_bytes;
    pte->zero_bytes = zero_bytes;
//    printf("pte->zero_bytes: %d\n",zero_bytes);
    pte->upage = upage;
    pte->writable = writable;
    pte->visited = false;
    struct thread* cur = thread_current();
    if (hash_insert(&cur->pt,&pte->elem)==NULL) return pte;
    return NULL;
}


bool palloc_stack (void *upage)
{
    struct page_table_entry *pte = malloc(sizeof(struct page_table_entry));
    pte->upage = pg_round_down(upage);
    pte->loaded = true;
    pte->type = SWAP;
    pte->visited = true;
    pte->writable = true;
    uint8_t *frame = palloc_frame (PAL_USER, pte);
    if (!frame || PHYS_BASE > pte->upage + (1 << 23)) {
        free(pte);
        return false;
    }
    if (!install_page(pte->upage, frame, pte->writable)) {
        free(pte);
        palloc_free(frame);
        return false;
    }
    pte->visited =!intr_context()? pte->visited : false;
    struct thread* cur = thread_current();
    if (hash_insert(&cur->pt, &pte->elem) == NULL) return true;
    return false;
}


struct page_table_entry* get_pte_by_upage(uint8_t *upage){
    struct thread* cur = thread_current();
    void* x = pg_round_down(upage);
    struct page_table_entry r;r.upage = x;
    struct hash_elem *e = hash_find(&cur->pt, &r.elem);
    if (e == NULL) return NULL;
    struct page_table_entry* ret = hash_entry (e, struct page_table_entry, elem);
    return ret;
}
bool load_file (struct page_table_entry *pte)
{
    enum palloc_flags flags;
    pte->loaded = true;
    if (pte->read_bytes == 0) flags =PAL_USER | PAL_ZERO;
        else flags = PAL_USER;
    void* frame = palloc_frame(flags, pte);
    if (!frame) return false;

    if (pte->read_bytes > 0)
    {
        lock_acquire(&file_lock);
        int size = file_read_at(pte->file, frame, pte->read_bytes, pte->ofs);
        lock_release(&file_lock);
        if (size == pte->read_bytes)
            memset(frame + pte->read_bytes, 0, pte->zero_bytes);
        else {
            palloc_free(frame);
            return false;
        }
    }
    if (!install_page(pte->upage, frame, pte->writable))
    {
        palloc_free(frame);
        return false;
    }
    pte->loaded = true;
    return pte->loaded;
}

bool load_swap(struct page_table_entry* pte){
    uint8_t *frame = palloc_frame (PAL_USER, pte);
    if (!frame || !install_page(pte->upage, frame, pte->writable))
    {
        palloc_free(frame);
        return false;
    }
    swap_in(pte->swap_index, pte->upage);
    pte->loaded = true;
    return true;
}


bool load_page(struct page_table_entry* pte){
    bool success = false;
    pte->visited = true;
    if (pte->loaded) return false;
    if (pte->type == FILE || pte->type == MMAP) success = load_file(pte);
    if (pte->type == SWAP) success = load_swap(pte);
    return success;
}












