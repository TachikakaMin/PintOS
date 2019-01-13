//
// Created by YIMIN TANG on 11/29/18.
//

#ifndef SRC_SWAP_H
#define SRC_SWAP_H

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "../lib/stddef.h"
#include "../threads/synch.h"
#include <bitmap.h>


void swap_init (void);
size_t swap_out (void *frame);
void swap_in (size_t used_index, void* frame);


#endif //SRC_SWAP_H
