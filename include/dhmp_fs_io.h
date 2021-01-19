#ifndef DHMP_FS_IO_H
#define DHMP_FS_IO_H
#include "dhmp_fs.h"
int dhmp_fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int dhmp_fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
void * rewrite_dirty_cache();

typedef struct writeThreadArgs{
	void * tbank;
	char * buf;
	int serverId;
	off_t offset;
	size_t size;
	int re;
	int bank_id;
}writeThreadArgs;

typedef struct writeThreadArgs2{
	int bank_id;
	 char * buf;
	 size_t size;
	 off_t offset;
}writeThreadArgs2;
#endif