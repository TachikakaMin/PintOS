//
// Created by YIMIN TANG on 11/28/18.
//

#ifndef SRC_FRAME_H
#define SRC_FRAME_H

#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "../threads/synch.h"
#include "../threads/palloc.h"
#include "../lib/kernel/list.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>


struct list tot_user_frame;
struct lock global_frame_lock;


struct frame_table_entry{
    void* frame;
    struct thread* owner_thread;
    struct page_table_entry* pte;
    struct list_elem elem;
};

void palloc_frame_init();
void* palloc_frame(enum palloc_flags flags,struct page_table_entry* pte);
void palloc_free(void* frame);


#endif //SRC_FRAME_H
