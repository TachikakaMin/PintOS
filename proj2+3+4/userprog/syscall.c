#include "userprog/syscall.h"
#include "filesys/file.h"
#include "../userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/debug.h"
#include "lib/syscall-nr.h"
#include "lib/kernel/stdio.h"
#include "vm/frame.h"
#include "../filesys/file.h"
#include "../lib/syscall-nr.h"
#include "../lib/kernel/stdio.h"
#include "../devices/input.h"
#include "../devices/shutdown.h"
#include "../filesys/filesys.h"
#include "../filesys/inode.h"
#include "../filesys/directory.h"

#define MAXNUM_SYS_CALL 20
static void syscall_handler (struct intr_frame *);
void write (struct intr_frame* f,int fd, const void *buffer, unsigned size);
void read (struct intr_frame* f,int fd, const void *buffer, unsigned size);
void sys_halt(struct intr_frame* f);     /* Halt */
void sys_exit(struct intr_frame* f);     /* Terminate process. */
void sys_exec(struct intr_frame* f);     /* Start another process. */
void sys_wait(struct intr_frame* f);     /* Wait for a child process */
void sys_create(struct intr_frame* f);   /* Create a file. */
void sys_remove(struct intr_frame* f);   /* Remove a file. */
void sys_open(struct intr_frame* f);     /* Open a file. */
void sys_filesize(struct intr_frame* f); /* Obtain a fileSize. */
void sys_read(struct intr_frame* f);     /* Read a file. */
void sys_write(struct intr_frame* f);    /* Write to a file. */
void sys_seek(struct intr_frame* f);     /* Change position in a file. */
void sys_tell(struct intr_frame* f);     /* Report current position in a file. */
void sys_close(struct intr_frame* f);    /* Close a file. */
void sys_mmap(struct intr_frame* f);    /* mmap. */
void sys_munmap(struct intr_frame* f);
void sys_mkdir(struct intr_frame* f);
void sys_chdir(struct intr_frame* f);
void sys_readdir(struct intr_frame* f);
void sys_inumber(struct intr_frame* f);
void sys_isdir(struct intr_frame* f);
void exit(const int status) ; /* exit curret thread with given status */

int file_tot=0;
int pdebug=0 ;

static void (*syscall_handlers[MAXNUM_SYS_CALL])(struct intr_frame *); /* a map for num to syscall*/

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  for (int i=0;i<MAXNUM_SYS_CALL;i++) syscall_handlers[i]=NULL;
  syscall_handlers[SYS_HALT] = &sys_halt;
  syscall_handlers[SYS_EXIT] = &sys_exit;
  syscall_handlers[SYS_EXEC] = &sys_exec;
  syscall_handlers[SYS_WAIT] = &sys_wait;
  syscall_handlers[SYS_CREATE] = &sys_create;
  syscall_handlers[SYS_REMOVE] = &sys_remove;
  syscall_handlers[SYS_OPEN] = &sys_open;
  syscall_handlers[SYS_READ] = &sys_read;
  syscall_handlers[SYS_FILESIZE] = &sys_filesize;
  syscall_handlers[SYS_WRITE] = &sys_write;
  syscall_handlers[SYS_SEEK] = &sys_seek;
  syscall_handlers[SYS_TELL] = &sys_tell;
  syscall_handlers[SYS_CLOSE] =&sys_close;
  syscall_handlers[SYS_MMAP] =&sys_mmap;
  syscall_handlers[SYS_MUNMAP] =&sys_munmap;
  syscall_handlers[SYS_MKDIR] = &sys_mkdir;
  syscall_handlers[SYS_CHDIR] = &sys_chdir;
  syscall_handlers[SYS_READDIR] = &sys_readdir;
  syscall_handlers[SYS_INUMBER] = &sys_inumber;
  syscall_handlers[SYS_ISDIR] = &sys_isdir;
  lock_init(&file_lock);
  file_tot = 2;
}




bool is_valid_pointer(void* esp,uint8_t argc,struct intr_frame * f);

static void
syscall_handler (struct intr_frame *f)
{
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    int syscall_num = * (int *)f->esp;
    if (syscall_num<0 || syscall_num >= MAXNUM_SYS_CALL || syscall_handlers[syscall_num]==NULL){
        exit(-1);
    }
    syscall_handlers[syscall_num](f);
    struct page_table_entry *pte = get_pte_by_upage(f->esp);
    if (pte) pte->visited = false;
}


struct fd_entry* get_File(struct thread * t,int fd);

void read (struct intr_frame* f,int fd, const void *buffer, unsigned size){
    if (pdebug==1) printf("read %d\n",thread_current()->tid);
    struct thread *cur = thread_current();
    if(fd==0){ // stdout
        for(unsigned int i=0;i<size;i++){
            *((char **)buffer)[i] = input_getc();
        }
        f->eax = size;
    }else{
        struct fd_entry *fn=NULL;
        fn = get_File(cur,fd);
        if(fn==NULL || fn->file->inode->data.is_dir)
        {
            f->eax= (uint32_t) -1;
            return;
        }
        if (fn->file == NULL) f->eax= (uint32_t) -1;
            else f->eax= (uint32_t) file_read(fn->file, (void *) buffer, size);
    }
}

void write (struct intr_frame* f,int fd, const void *buffer, unsigned size){
  if (pdebug==1) printf("write %d\n",thread_current()->tid);
  struct thread *cur = thread_current();
  if(fd==1){ // stdout
    putbuf((char *) buffer,(size_t)size);
    f->eax = (int)size;
  }else{
      struct fd_entry *fn=NULL;
      fn = get_File(cur,fd);
      if(fn==NULL || fn->file->inode->data.is_dir)
      {
          f->eax= (uint32_t) -1;
          return ;
      }
      if (fn->file == NULL) f->eax = (uint32_t) -1;
        else f->eax= (uint32_t) file_write(fn->file, buffer, size);
  }
}


int open (const char *file);

int close (const int fd);

int close_all(struct thread* t){
    if (pdebug==1) printf("close all %d\n",thread_current()->tid);
    struct list_elem* e;
    while (!list_empty(&t->have_file)){
        e=list_begin (&t->have_file);
        struct fd_entry *x = list_entry(e, struct fd_entry, elem);
        list_remove(&x->elem);
        file_close(x->file);
        free(x);
    }
    return 1;
}

void sys_halt(struct intr_frame* f UNUSED){
    if (pdebug==1) printf("sys_halt %d\n",thread_current()->tid);
    shutdown();
}

void sys_exit(struct intr_frame* f){
    if (pdebug==1) printf("sys_exit %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    int status = *(int *)(f->esp +4);
    exit(status);
}

static int
get_user (const uint8_t *uaddr)
{
    //printf("%s\n", "call get user");
    if(!is_user_vaddr((void *)uaddr)){
        return -1;
    }
    if(pagedir_get_page(thread_current()->pagedir,uaddr)==NULL){
        return -1;
    }
    //printf("%s\n","is_user_vaddr" );
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
    : "=&a" (result) : "m" (*uaddr));
    return result;
}

bool is_valid_string(void *str);

void sys_exec(struct intr_frame* f){
    if (pdebug==1) printf("sys_exec %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    char *file_name = *(char **)(f->esp+4);
    if(!is_valid_string(file_name)){
        exit(-1);
    }
    lock_acquire(&file_lock);
    f->eax = (uint32_t) process_execute(file_name);
    lock_release(&file_lock);
}




void sys_mkdir(struct intr_frame* f){
    if (pdebug==1) printf("sys_mkdir %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    char *file_name = *(char **)(f->esp+4);
    if(!is_valid_string(file_name)){
        exit(-1);
    }
    lock_acquire(&file_lock);
    f->eax = (uint32_t) filesys_create(file_name,0,true);
    lock_release(&file_lock);
}
void sys_chdir(struct intr_frame* f){
    if (pdebug==1) printf("sys_chdir %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    char *file_name = *(char **)(f->esp+4);
    if(!is_valid_string(file_name)){
        exit(-1);
    }
    lock_acquire(&file_lock);
    f->eax = (uint32_t) filesys_chdir(file_name);
    lock_release(&file_lock);
}

void sys_wait(struct intr_frame* f) {
  if (pdebug==1) printf("sys_wait %d\n",thread_current()->tid);
  if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
  int tid = *((int*)f->esp+1);
  f->eax = (uint32_t) process_wait(tid);
}


void sys_create(struct intr_frame* f){
    if (pdebug==1) printf("sys_create %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    char* file_name = *(char **)(f->esp+4);
    if(!is_valid_string(file_name)){
        exit(-1);
    }
    int size = *(int *)(f->esp + 8);
    lock_acquire(&file_lock);
    f->eax = (uint32_t) filesys_create(file_name, size, false);
    lock_release(&file_lock);
}


void sys_remove(struct intr_frame* f){
    if (pdebug==1) printf("sys_remove %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    char *file_name = *(char **)(f->esp+4);
    f->eax = (uint32_t) filesys_remove(file_name);
}

void sys_open(struct intr_frame* f){
    if (pdebug==1) printf("sys_open %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    char *file_name = *(char **)(f->esp+4);
    if(!is_valid_string(file_name)){
        exit(-1);
    }
    lock_acquire(&file_lock);
    f->eax = (uint32_t) open(file_name);
    lock_release(&file_lock);
}


static struct file *
find_file_by_fd (int fd)
{
    struct thread* cur = thread_current();
    struct list_elem* e;
    for (e = list_begin (&cur->have_file); e != list_end (&cur->have_file); e = list_next (e)) {
        struct fd_entry* x = list_entry(e,struct fd_entry,elem);
        if (x->fd == fd) {
            return x->file;
        }
    }
    return NULL;
}

void sys_filesize(struct intr_frame* f){
    if (pdebug==1) printf("sys_filesize %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp +4, 4,f)) exit(-1);
    int fd = *(int *)(f->esp + 4);
    struct file *fi = find_file_by_fd(fd);
    if(fi == NULL){
        exit(-1);
    }
    f->eax = (uint32_t) file_length(fi);
}

void sys_read(struct intr_frame* f){
    if (pdebug==1) printf("sys_read %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp + 4, 12,f)) exit(-1);
    int fd = *(int *)(f->esp +4);
    void *buffer = *(char**)(f->esp + 8);
    unsigned size = *(unsigned *)(f->esp + 12);
    if (!is_valid_pointer(buffer, 1,f) || !is_valid_pointer(buffer + size,1,f)){
        exit(-1);
    }
    lock_acquire(&file_lock);
    read(f,fd, buffer, size);
    lock_release(&file_lock);
}


bool isdir(int fd){
    struct thread* cur = thread_current();
    struct fd_entry *fde = get_File(cur,fd);
    return fde->file->inode->data.is_dir;
}

void sys_isdir(struct intr_frame* f){
    if (pdebug==1) printf("sys_isdir %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp + 4, 12,f)) exit(-1);
    int fd = *(int *)(f->esp +4);
    lock_acquire(&file_lock);
    f->eax = isdir(fd);
    lock_release(&file_lock);
}


bool readdir(int fd, char *name) {

    struct thread* cur = thread_current();
    struct fd_entry *fde = get_File(cur,fd);
    if (fde == NULL) return false;
    struct inode *inode = fde->file->inode;
    if (inode == NULL) return false;
    if (!inode->data.is_dir) return false;
    struct dir_entry e;
    while (inode_read_at (inode, &e, sizeof e, fde->file->pos) == sizeof e)
    {
        fde->file->pos += sizeof e;
        if (e.in_use)
        {
            strlcpy (name, e.name, NAME_MAX + 1);
            return true;
        }
    }
    return false;
}

void sys_readdir(struct intr_frame* f){
    if (pdebug==1) printf("sys_readdir %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp + 4, 12,f)) exit(-1);
    int fd = *(int *)(f->esp +4);
    char *buffer = *(char**)(f->esp + 8);
    lock_acquire(&file_lock);
    f->eax = (uint32_t) readdir(fd, buffer);
    lock_release(&file_lock);
}

void sys_write(struct intr_frame* f){
    if (pdebug==1) printf("sys_write %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp + 4, 12,f))exit(-1);
    int fd = *(int *)(f->esp +4);
    char *buffer = *(char**)(f->esp + 8);
    unsigned size = *(unsigned *)(f->esp + 12);
    if (!is_valid_pointer(buffer, 1,f) || !is_valid_pointer(buffer + size,1,f)){
        exit(-1);
    }
    lock_acquire(&file_lock);
    write(f,fd, buffer, size);
    lock_release(&file_lock);
}


void sys_seek(struct intr_frame* f){
    if (pdebug==1) printf("sys_seek %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp +4, 8,f)) exit(-1);
    int fd = *(int *)(f->esp + 4);
    unsigned pos = *(unsigned *)(f->esp + 8);
    struct file *fi = find_file_by_fd(fd);
    if(fi == NULL) exit(-1);
    file_seek(fi,pos);
}

void sys_tell(struct intr_frame* f){
    if (pdebug==1) printf("sys_tell %d\n",thread_current()->tid);
    if (!is_valid_pointer(f->esp+4, 4,f)) exit(-1);
    int fd = *(int *)(f->esp + 4);
    struct file *fi = find_file_by_fd(fd);
    if(fi == NULL) exit(-1);
    f->eax = (uint32_t) file_tell(fi);
}



void sys_close(struct intr_frame* f){
    if (pdebug==1) printf("sys_close %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    int fd = *(int *)(f->esp + 4);
    if (!close(fd)) exit(-1);
}


bool add_mmap_to_process (struct page_table_entry *pte)
{
    struct thread* cur = thread_current();
    struct mmap_file* mm =(struct mmap_file* ) malloc(sizeof(struct mmap_file));
    if (mm) {
        mm->mmap_num = cur->mmap_num;
        mm->pte = pte;
        list_push_back(&cur->mmap_list, &mm->elem);
    }
    if (mm) return true;
    return false;
}

bool add_mmap_to_page_table(struct file *file, int32_t ofs, uint8_t *upage,
                            uint32_t read_bytes, uint32_t zero_bytes)
{
    struct thread* cur = thread_current();
    struct page_table_entry *pte =( struct page_table_entry*) malloc(sizeof(struct page_table_entry));
    if (!pte) return false;
    pte->file = file;
    pte->ofs = ofs;
    pte->upage = upage;
    pte->read_bytes = read_bytes;
    pte->zero_bytes = zero_bytes;
    pte->loaded = false;
    pte->type = MMAP;
    pte->writable = true;
    pte->visited = false;

    if (!add_mmap_to_process(pte))
    {
        free(pte);
        return false;
    }

    if (hash_insert(&cur->pt, &pte->elem))
    {
        pte->type = -1;
        return false;
    }
    return true;
}


void munmap (int mapping)
{
    struct thread *t = thread_current();
    struct list_elem *e = list_begin(&t->mmap_list);
    struct list_elem *e_next;
    struct file *f = NULL;
    int close = 0;
    while (e != list_end (&t->mmap_list))
    {
        e_next = list_next(e);
        struct mmap_file *mm = list_entry (e, struct mmap_file, elem);
        if (mm->mmap_num == mapping || mapping == -1)
        {
            mm->pte->visited = true;
            if (mm->pte->loaded)
            {
                if (!pagedir_is_dirty(t->pagedir, mm->pte->upage))
                {
                    palloc_free(pagedir_get_page(t->pagedir, mm->pte->upage));
                    pagedir_clear_page(t->pagedir, mm->pte->upage);
                }
                else {
                    lock_acquire(&file_lock);
                    file_write_at(mm->pte->file, mm->pte->upage, mm->pte->read_bytes, mm->pte->ofs);
                    lock_release(&file_lock);
                }
            }
            if (mm->pte->type != -1) hash_delete(&t->pt, &mm->pte->elem);
            list_remove(&mm->elem);
            if (mm->mmap_num != close)
            {
                if (f)
                {
                    lock_acquire(&file_lock);
                    file_close(f);
                    lock_release(&file_lock);
                }
                close = mm->mmap_num;
                f = mm->pte->file;
            }
            free(mm->pte);
            free(mm);
        }
        e = e_next;
    }
    if (f)
    {
        lock_acquire(&file_lock);
        file_close(f);
        lock_release(&file_lock);
    }
}

int mmap(int fd, void* addr){
    struct thread* cur = thread_current();
    struct fd_entry *fde = get_File(cur,fd);
    if (fde == NULL) return -1;
    if (fde->file==NULL || addr == NULL || ((uint32_t) addr % PGSIZE) != 0) return -1;
    struct file *file = file_reopen(fde->file);
    if (!file || file_length(fde->file) == 0) return -1;
    off_t ofs = 0;
    size_t read_bytes = (size_t) file_length(file);
    ++cur->mmap_num;
    while (read_bytes > 0)
    {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        if (!add_mmap_to_page_table(file, ofs, (uint8_t *) addr, (uint32_t) page_read_bytes, (uint32_t) page_zero_bytes))
        {
            munmap(cur->mmap_num);
            return -1;
        }
        read_bytes -= page_read_bytes;
        ofs += page_read_bytes;
        addr += PGSIZE;
    }
    return cur->mmap_num;
}


void sys_mmap(struct intr_frame* f){
    if (pdebug==1) printf("sys_mmap %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,8,f)) exit(-1);
    int fd = *(int *)(f->esp +4);
    int* addr = f->esp + 8;
    if (!is_valid_pointer(addr, 1,f))
        exit(-1);
    f->eax = (uint32_t) mmap(fd, (void *) *addr);
}


void sys_munmap(struct intr_frame* f){
    if (pdebug==1) printf("sys_mmap %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    int fd = *(int *)(f->esp +4);
    munmap(fd);
}

int inumber(int fd){
    struct thread* cur = thread_current();
    struct fd_entry *fde = get_File(cur,fd);
    struct inode* inode = fde->file->inode;
    return inode->sector;
}


void sys_inumber(struct intr_frame* f){
    if (pdebug==1) printf("sys_inumber %d\n",thread_current()->tid);
    if(!is_valid_pointer(f->esp+4,4,f)) exit(-1);
    int fd = *(int *)(f->esp +4);
    lock_acquire (&file_lock);
    f->eax = (uint32_t) inumber(fd);
    lock_release (&file_lock);
}

void exit(const int status){
    if (pdebug==1) printf("exit %d\n",thread_current()->tid);
    struct thread *cur = thread_current();
    cur->ret = status;
    struct ret_data* f_ret =(struct ret_data*) malloc(sizeof(struct ret_data));
    f_ret->tid = cur->tid;
    f_ret->ret = cur->ret;
    f_ret->vis = 0;
    list_push_back(&cur->father->son_ret,&f_ret->elem);
    thread_exit();
}

bool is_valid_string(void *str) {
    //return true;
    int ch;
    while((ch=get_user((uint8_t*)str++))!='\0' && ch!=-1);
    if(ch=='\0')
        return true;
    else
        return false;
}


bool is_valid_pointer(void *esp, uint8_t argc, struct intr_frame * f) {
    bool b = true;
    for (uint8_t i = 0; i < argc; ++i) {
        if (!is_user_vaddr(esp)) return false;
        if (pagedir_get_page(thread_current()->pagedir, esp) == NULL) {
            b = false;
            break;
        }
    }
    if (b == false){
        struct page_table_entry *pte = get_pte_by_upage(esp);
        if (pte) b = load_page(pte);
        else {
            if (esp >= f->esp - 32)
                b = palloc_stack(esp);
        }
    }
    return b;
}

int open(const char *file) {
    if (pdebug==1) printf("open %d\n",thread_current()->tid);
    struct thread* cur = thread_current();
    struct file* fd = filesys_open(file);
    if (fd == NULL){
        return -1;
    }
    struct fd_entry* x = (struct fd_entry*)malloc(sizeof(struct fd_entry));
    x->fd = ++file_tot;
    x->file = fd;
    list_push_back(&cur->have_file,&x->elem);
    return x->fd;
}

int close(const int fd) {
    if (pdebug==1) printf("close %d\n",thread_current()->tid);
    struct thread* cur = thread_current();
    struct list_elem* e;
    for (e = list_begin (&cur->have_file); e != list_end (&cur->have_file); e = list_next (e)) {
        struct fd_entry* x = list_entry(e,struct fd_entry,elem);
        if (x->fd == fd) {
            list_remove(&x->elem);
            file_close(x->file);
            free(x);
            return 1;
        }
    }
    return 0;
}

struct fd_entry *get_File(struct thread *t, int fd) {
    struct list_elem *e;

    for (e = list_begin (&t->have_file); e != list_end (&t->have_file);e=list_next (e))
    {
        struct fd_entry *fn =  list_entry (e, struct fd_entry, elem);
        if(fn->fd==fd)
            return fn;
    }
    return NULL;

}
