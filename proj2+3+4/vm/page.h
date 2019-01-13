//
// Created by YIMIN TANG on 11/29/18.
//

#ifndef SRC_PAGE_H
#define SRC_PAGE_H
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
#include "vm/swap.h"
#include "lib/kernel/hash.h"
#include "../lib/kernel/hash.h"
#include "../filesys/file.h"
#include "../threads/interrupt.h"
#include "../threads/malloc.h"
#include "../threads/palloc.h"
#include "../threads/thread.h"
#include "../threads/vaddr.h"
#include "../userprog/pagedir.h"
#include "../userprog/process.h"
#include "../userprog/syscall.h"
#include "../vm/frame.h"
#include "../vm/swap.h"

#define FILE 1
#define SWAP 2
#define MMAP 3
struct page_table_entry{
    struct file* file;
    int type;
    off_t ofs;
    bool visited;
    size_t swap_index;
    uint8_t *upage;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
    bool loaded;
    struct hash_elem elem;
};

struct page_table_entry* add_to_page_table(struct file *file, off_t ofs, uint8_t *upage,
                                           uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void init_page_table(struct thread* t);
static bool page_hash_less (const struct hash_elem *x, const struct hash_elem *y, void *aux UNUSED);
static unsigned page_hash_func (const struct hash_elem *e, void *aux UNUSED);
bool palloc_stack (void *upage);
struct page_table_entry* get_pte_by_upage(uint8_t *upage);
bool load_page(struct page_table_entry* pte);
#endif //SRC_PAGE_H
