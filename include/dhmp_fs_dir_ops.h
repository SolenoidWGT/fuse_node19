#ifndef DHMP_FS_DIR_OPS_H
#define DHMP_FS_DIR_OPS_H

#include "dhmp_fs.h"

 int dhmp_fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
 int dhmp_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flag);
 int dhmp_fs_mknod(const char *path, mode_t mode, dev_t rdev);
 int dhmp_fs_mkdir(const char *path, mode_t mode);
 int dhmp_fs_rmdir(const char *path);
 int dhmp_fs_unlink(const char *path);
 int dhmp_fs_rename(const char *from, const char *to, unsigned int flags);
 int dhmp_fs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
 int dhmp_fs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
 int dhmp_fs_truncate(const char *path, off_t size, struct fuse_file_info *fi);
 int dhmp_fs_open(const char *path, struct fuse_file_info *fi);
 int dhmp_fs_statfs(const char *path, struct statvfs *stbuf);
 int dhmp_fs_access(const char *path, int mask);


int dhmpCreateDirectory(const char * path);
int dhmpGetAttr(const char *path,struct attr *attr);
struct inode_list * dhmpReadDir(const char *path);
int dhmpMknod(const char * path);
void deal(const char *path,char * dirname,char * filename);
struct inode * get_father_inode(char *dirname);

inode * search_d_hash(const char * path);
#endif