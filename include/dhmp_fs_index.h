#ifndef DHMP_FS_INDEX_H
#define DHMP_FS_INDEX_H

#include "../include/dhmp_fs.h"

indirect_class1 * init_indirect_class1();
indirect_class2 * init_indirect_class2();
indirect_class3 * init_indirect_class3();

void free_indirect_class1(inode * head);
void free_indirect_class2(inode * head);
void free_indirect_class3(inode * head);

context * level3_local_index(off_t* offset, inode* head, int only_read);
context * get_free_context();
int getFreeChunk();

void init_inode_slab();
void reset_indoe(inode * now);
inode* get_free_inode();
inode* malloc_inode();
#endif