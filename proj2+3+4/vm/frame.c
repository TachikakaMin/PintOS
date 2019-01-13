//
// Created by YIMIN TANG on 11/28/18.
//
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "../lib/kernel/list.h"
#include "frame.h"
#include "../threads/palloc.h"
#include "../threads/synch.h"
#include "../lib/debug.h"
#include "../threads/malloc.h"
#include "../threads/thread.h"
#include "../userprog/pagedir.h"
#include "../filesys/file.h"
#include "../lib/stdio.h"

void palloc_frame_init(){
    list_init(&tot_user_frame);
    lock_init(&global_frame_lock);
}


void* frame_evict (enum palloc_flags flags)
{
    lock_acquire(&global_frame_lock);
    struct list_elem *e;
    for (e =list_begin(&tot_user_frame);e!=list_end(&tot_user_frame);e = list_next(e)) {
        struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
        if (!fte->pte->visited) {
            struct thread *t = fte->owner_thread;
            if (!pagedir_is_accessed(t->pagedir, fte->pte->upage)) continue;
            pagedir_set_accessed(t->pagedir, fte->pte->upage, false);
            bool b = pagedir_is_dirty(t->pagedir, fte->pte->upage) || fte->pte->type == SWAP;
            if (b) {
                if (fte->pte->type == MMAP) {
                    lock_acquire(&global_frame_lock);
                    file_write_at(fte->pte->file, fte->frame, fte->pte->read_bytes, fte->pte->ofs);
                    lock_release(&global_frame_lock);
                }
                else fte->pte->swap_index = swap_out(fte->frame);
            }
            fte->pte->loaded = false;
            if (b && fte->pte->type!=MMAP) fte->pte->type = SWAP;
            list_remove(&fte->elem);
            pagedir_clear_page(t->pagedir, fte->pte->upage);
            palloc_free_page(fte->frame);
            free(fte);
            lock_release(&global_frame_lock);
            return palloc_get_page(flags);
        }
    }
    lock_release(&global_frame_lock);
    return NULL;
}



void* palloc_frame(enum palloc_flags flags,struct page_table_entry* pte){
    int err = flags & PAL_USER;
    if (!err) return NULL;
    void* frame = palloc_get_page(flags);
    if (!frame) frame = frame_evict(flags);
    struct frame_table_entry* fte = (struct frame_table_entry*) malloc(sizeof(struct frame_table_entry));
    if (fte == NULL) return NULL;
    lock_acquire(&global_frame_lock);
    list_push_back(&tot_user_frame,&fte->elem);
    lock_release(&global_frame_lock);
    fte->pte = pte;
    fte->frame = frame;
    fte->owner_thread = thread_current();
    return frame;
}

void palloc_free(void* frame){
    if (frame == NULL) return;
    struct list_elem *e;
    lock_acquire(&global_frame_lock);
    for (e = list_begin(&tot_user_frame); e != list_end(&tot_user_frame); e = list_next(e))
    {
        struct frame_table_entry *fte = list_entry(e, struct frame_table_entry, elem);
        if (fte->frame != frame) continue;
        list_remove(e);
        free(fte);
        palloc_free_page(frame);
        lock_release(&global_frame_lock);
        return;
    }
    lock_release(&global_frame_lock);
    return;
}


