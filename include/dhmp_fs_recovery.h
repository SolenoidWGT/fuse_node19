#ifndef DHMP_FS_RECOVERY_H
#define DHMP_FS_RECOVERY_H
#include "dhmp_fs.h"
void fuse_recover(int re_server_id);
#ifdef DHMP_ON
server_node * find_alive_server();
#endif
#endif