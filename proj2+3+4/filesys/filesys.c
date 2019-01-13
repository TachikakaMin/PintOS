#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "inode.h"
#include "filesys/directory.h"
#include "directory.h"
#include "file.h"
#include "free-map.h"
#include "../devices/block.h"
#include "../lib/debug.h"
#include "filesys.h"
#include "sector-buffer.h"
#include "../threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  flush_all_sector();
}


char* get_other_except_name(const char *name)
{
  char* argv[10];
  int argc=0;
  char* token = NULL;
  token = (char *)malloc(strlen(name)+1);
    token[0] = '\0';
  char* save_ptr = NULL,*t = NULL;
  strlcpy(token,name,PGSIZE);
  t = strtok_r(token,"/",&save_ptr);
  argv[argc] = t;
  while (t != NULL) {
    (argc)++;
    t = strtok_r(NULL,"/",&save_ptr);
    argv[argc] = t;
  }
  char* cNewStr = (char*)malloc(sizeof(char)*20);
  cNewStr[0]='\0';
  if (name[0]=='/') {
    strlcat(cNewStr, "/", PGSIZE);
  }
  for (int i=0;i<argc-1;i++) {
    strlcat(cNewStr, argv[i], PGSIZE);
    strlcat(cNewStr, "/", PGSIZE);
  }
    free(token);
  if (argc-1<=0)
  {
    free(cNewStr);
    return NULL;
  }
//  printf("%s\n",cNewStr);
  return cNewStr;
}


/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */

void only_get_name(const char * name,const char* dst)
{
    char *token, *p, *last_token = "";
    char *s = (char*) malloc( sizeof(char) * (strlen(name) + 1) );
    memcpy (s, name, sizeof(char) * (strlen(name) + 1));
    for (token = strtok_r(s, "/", &p); token != NULL;
         token = strtok_r(NULL, "/", &p))
        last_token = token;
    memcpy (dst, last_token, sizeof(char) * (strlen(last_token) + 1));
    free(s);
}

bool
filesys_create (const char *name, off_t initial_size,bool is_dir)
{
  block_sector_t inode_sector = 0;
  struct dir *dir;
  char new_name[strlen(name)+1];
  only_get_name(name,new_name);
  const char *d = get_other_except_name(name);
  dir = get_dir_by_name(d,name);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size,is_dir)
                  && dir_add (dir, new_name, inode_sector,is_dir));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);
  return success;
}



/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir;
  if (strlen(name)<=0) return NULL;
  char new_name[strlen(name)+1];
  only_get_name(name,new_name);
//    printf("%s\n",new_name);
  char *d = NULL;
  if (name[strlen(name)-1] == '/') d = name;
    else d = get_other_except_name(name);
//  if (d!=NULL) printf("%s\n",d);
  dir = get_dir_by_name(d,name);

  struct inode *inode = NULL;

  if (dir != NULL)
      if (strlen(new_name)>0) dir_lookup (dir, new_name, &inode);
        else inode = dir_get_inode (dir);
  dir_close (dir);
  if (inode==NULL || inode->removed) return NULL;
  return file_open (inode);
}




bool filesys_chdir(char* name)
{
    struct dir *dir;
//  char new_name[strlen(name)+1];
//  only_get_name(name,new_name);
//  printf("%s\n",new_name);
//  char *d = NULL;
//  if (name[strlen(name)-1] == '/') d = name;
//  else d = get_other_except_name(name);
//  if (d!=NULL) printf("%s\n",d);
    dir = get_dir_by_name(name,name);
    struct thread* cur = thread_current();
    if (dir == NULL) return false;
    dir_close (cur->now_dir);
    cur->now_dir = dir;
    return true;
}


/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  char new_name[strlen(name)+1];
  only_get_name(name,new_name);
//  printf("%s\n",new_name);
  char *d = NULL;
  if (name[strlen(name)-1] == '/')
  {
    d = (char*)malloc(sizeof(char)*strlen(name)+1);
    d[0]='0';
    strlcpy(d,name,strlen(name));
  }
  else d = get_other_except_name(name);
//  if (d!=NULL) printf("%s\n",d);
  dir = get_dir_by_name(d,name);

  bool success = dir != NULL && dir_remove (dir, new_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
