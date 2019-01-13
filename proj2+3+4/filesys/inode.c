#include "filesys/inode.h"
#include "inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "../devices/block.h"
#include "../lib/kernel/list.h"
#include "../threads/malloc.h"
#include "filesys.h"
#include "free-map.h"
#include "../lib/debug.h"
#include "off_t.h"
#include "../lib/round.h"
#include "sector-buffer.h"
#include "../threads/thread.h"
#include "directory.h"

int tdebug = 0;

int min(int x,int y){if (x<y) return x;return y;}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return (size_t) DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}




block_sector_t find_sector_num_in_levels(const struct inode_disk* disk_inode,off_t index){
    uint32_t sector_level2[FULLLEVEL];
    uint32_t sector_level3[FULLLEVEL];
//    if (debug) printf("find_in_level1\n");
    if (index<LEVEL1) return disk_inode->sector_level1[index];
    index-=LEVEL1;

    if (tdebug) printf("find_in_level2\n");
    if (index<FULLLEVEL) {
        cache_buffer_read(fs_device, disk_inode->sector_level2 , sector_level2);
        //printf("index: %d, sector_level2[index]: %d\n",index,sector_level2[index]);
        return sector_level2[index];
    }
    index-=FULLLEVEL;

    if (tdebug) printf("find_in_level3\n");
    if (index<FULLLEVEL*FULLLEVEL)
    {
        int x = index/FULLLEVEL;
        int mod = index%FULLLEVEL;
        cache_buffer_read(fs_device, disk_inode->sector_level3 , sector_level2);
        cache_buffer_read(fs_device, sector_level2[x] , sector_level3);
        return sector_level3[mod];
    }
    index -= FULLLEVEL*FULLLEVEL;

    printf("How Large Are You?\n");
    return -1;
}


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return find_sector_num_in_levels(&inode->data ,pos/BLOCK_SECTOR_SIZE);
  else
    return (block_sector_t) -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
    for (int i=0;i<FULLLEVEL;i++) zeros[i]=0;
}

bool alloc_for_level1(int l,struct inode_disk *disk_inode)
{
  for (int i = 0; i < LEVEL1; ++ i) {
      if (l<=0) break;
    if (disk_inode->sector_level1[i] == 0) {
        l--;
      if(!free_map_allocate (1, &disk_inode->sector_level1[i])) return false;
      cache_buffer_write (fs_device ,disk_inode->sector_level1[i], zeros);
    }
  }
  return true;
}


bool alloc_for_level2(int l,struct inode_disk *disk_inode)
{
    uint32_t sector_level2[FULLLEVEL];

    if (disk_inode->sector_level2 != 0) cache_buffer_read(fs_device, disk_inode->sector_level2 , sector_level2);
    else {
        if(!free_map_allocate (1, &disk_inode->sector_level2)) return false;
        cache_buffer_write(fs_device,disk_inode->sector_level2,  zeros);
        cache_buffer_read (fs_device, disk_inode->sector_level2 , sector_level2);
    }

    for (int i=0;i<FULLLEVEL;i++) {
        if (l<=0) break;
        if (sector_level2[i]==0) {
            if (!free_map_allocate(1, &sector_level2[i])) return false;
            cache_buffer_write(fs_device, sector_level2[i], zeros);
            l--;
        }
    }
    cache_buffer_write(fs_device, disk_inode->sector_level2, sector_level2);
    return true;
}

bool alloc_for_level3(int l,struct inode_disk *disk_inode)
{
  uint32_t sector_level2[FULLLEVEL];
  uint32_t sector_level3[FULLLEVEL];
  if (disk_inode->sector_level3 != 0) cache_buffer_read(fs_device, disk_inode->sector_level3 , sector_level2);
  else {
    if(!free_map_allocate (1, &disk_inode->sector_level3)) return false;
    cache_buffer_write(fs_device,disk_inode->sector_level3,  zeros);
    cache_buffer_read (fs_device, disk_inode->sector_level3 , sector_level2);
  }

  for (int i=0;i<FULLLEVEL ;i++) {
      if (l<=0) break;
      if (sector_level2[i] != 0) cache_buffer_read(fs_device, sector_level2[i], sector_level3);
      else {
          if (!free_map_allocate(1, &sector_level2[i])) return false;
          cache_buffer_write(fs_device, sector_level2[i], zeros);
          cache_buffer_read(fs_device, sector_level2[i], sector_level3);
      }
      for (int j = 0; j < FULLLEVEL; j++) {
          if (l<=0) break;
          if (sector_level3[j] == 0) {
              l--;
              //if (l<527) printf("i: %d\n",i);
              if (!free_map_allocate(1, &sector_level3[j])) return false;
              //if (l<527) printf("j: %d\n",j);
              cache_buffer_write(fs_device, sector_level3[j], zeros);
          }
      }
      //printf("l :%d\n",l);
      cache_buffer_write(fs_device, sector_level2[i], sector_level3);
  }
    //printf("end.\n");
    cache_buffer_write(fs_device, disk_inode->sector_level3 ,sector_level2);
  return true;
}


bool alloc_sector_for_inode(size_t sectors,struct inode_disk *disk_inode)
{
    if (tdebug) printf("sectors: %d\n",sectors);
//    if (tdebug) printf("LEVEL1\n");
    int l = min((int) sectors, LEVEL1);
    bool re = alloc_for_level1(l,disk_inode);
    if (re == false) return re;
//    if (tdebug) printf("LEVEL1 YES!\n");
    if (sectors <= LEVEL1) return re;
    sectors-=LEVEL1;


    if (tdebug) printf("LEVEL2\n");
    l = min((int) sectors, FULLLEVEL);
    if (tdebug) printf("length: %d\n",l);
    re = alloc_for_level2(l,disk_inode);
    if (re == false) return re;
    if (tdebug) printf("LEVEL2 YES!\n");
    if (sectors <= FULLLEVEL) return re;
    sectors -= FULLLEVEL;


    if (tdebug) printf("LEVEL3\n");
    l = min((int) sectors, FULLLEVEL * FULLLEVEL);
    if (tdebug) printf("length: %d\n",l);
    re = alloc_for_level3(l,disk_inode);
    if (re == false) return re;
    if (tdebug) printf("LEVEL3 YES\n");
    if (sectors <= FULLLEVEL*FULLLEVEL) return re;

    if (tdebug) printf("How Large Are You?\n");
    return false;
}


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length,bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  struct thread* cur = thread_current();
  ASSERT (length >= 0);
  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
        disk_inode->is_dir = is_dir;
      disk_inode->magic = INODE_MAGIC;
//      if (free_map_allocate (sectors, &disk_inode->start))
      if (alloc_sector_for_inode(sectors,disk_inode))
        {
            cache_buffer_write (fs_device, sector, disk_inode);
//          if (sectors > 0)
//            {
//              static char zeros[BLOCK_SECTOR_SIZE];
//              size_t i;
//
//              for (i = 0; i < sectors; i++)
//                  cache_buffer_write (fs_device, (block_sector_t) (disk_inode->start + i), zeros);
//            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {

      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }
  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  if (thread_current()->now_dir == NULL)  inode->father = NULL;
    else inode->father = thread_current()->now_dir;
  cache_buffer_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}


void free_inode(struct inode * inode){
    uint32_t sector_level2[FULLLEVEL];
    uint32_t sector_level3[FULLLEVEL];
    off_t file_length = inode->data.length;
    if(file_length < 0) return ;
    int num_sectors = (int) bytes_to_sectors(file_length);
    int l = min(num_sectors,LEVEL1);
    for (int i=0;i<l;i++) free_map_release (inode->data.sector_level1[i], 1);
    num_sectors-=l;


    l = min(num_sectors,FULLLEVEL);
    cache_buffer_read(fs_device, inode->data.sector_level2, sector_level2);
    for (int i=0;i<l;i++) free_map_release (sector_level2[i], 1);
    num_sectors-=l;


    l = min(num_sectors,FULLLEVEL*FULLLEVEL);
    cache_buffer_read(fs_device, inode->data.sector_level3, sector_level2);
    for (int i=0;i<FULLLEVEL;i++)
    {
        if (sector_level2[i]==0) return ;
        cache_buffer_read(fs_device, sector_level2[i], sector_level3);
        for (int j=0;j<FULLLEVEL;j++) {
            if (sector_level3[j]==0) return ;
            free_map_release (sector_level3[j], 1);
        }
    }

    num_sectors-=l;
    if (num_sectors>0) printf("How Large Are You?\n");
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_inode(inode);
        }

      free (inode);

    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
            cache_buffer_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
            cache_buffer_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;


    if( byte_to_sector(inode, offset + size - 1) == (uint32_t) -1 ) {
        if (!alloc_sector_for_inode (bytes_to_sectors(offset + size),&inode->data)) return 0;
        inode->data.length = offset + size;
        cache_buffer_write (fs_device, inode->sector, &inode->data);
    }


  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
            cache_buffer_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
              cache_buffer_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
            cache_buffer_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

