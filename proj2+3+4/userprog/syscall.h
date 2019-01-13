#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "../lib/stdint.h"
#include "../lib/kernel/list.h"
#include "../threads/malloc.h"
#include "../filesys/file.h"
#include "../threads/thread.h"
struct fd_entry{
    int fd;
    struct file *file;
    struct list_elem elem;
};

struct ret_data
{
    int tid;
    int ret;
    bool vis;
    struct list_elem elem;
};

struct mmap_file {
    struct page_table_entry *pte;
    int mmap_num;
    struct list_elem elem;
};

void syscall_init (void);
int close_all(struct thread* t);
#endif /* userprog/syscall.h */
