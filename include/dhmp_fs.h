#ifndef DHMP_FS_H
#define DHMP_FS_H

#define FUSE_USE_VERSION 31

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#ifdef linux
#define _XOPEN_SOURCE 700
#endif
#include <sys/time.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


#define DHMP_ON 1
// #define SSD_TEST 1
// #define CACHE_ON 1


#include "../include/list.h"
#include "../include/dhmp_fs_hashmap.h"

#ifdef DHMP_ON
#include "../dhmp_client/include/dhmp.h"
#endif




//#define TOTOL_SIZE ((uint64_t)1024*1024*1024*2)     // 磁盘总空间,2G
#define TOTOL_SIZE ((uint64_t)1024*1024*1024*8)     // 磁盘总空间,2M
//#define BANK_SIZE (1024*1024*4)                     // BANK大小，4M
#define BANK_SIZE ((uint64_t)1024*1024*4)          // BANK大小，4M
#define BANK_NUM (TOTOL_SIZE/BANK_SIZE)             // dhmp_malloc的数量
#define CHUNK_SIZE ((uint64_t)1024*4)           // 单个数据块的大小，16M
#define CHUNK_NUM (TOTOL_SIZE/CHUNK_SIZE)           // 数据块的总数
#define CACHE_SIZE_BYTE ((uint64_t)1024*1024*512)	

#define FILE_NAME_LEN 512

#define ACCESS_SIZE 65536
#define DHMP_ADDR_LEN 18

#define INDIRECT_INDEX_NUMS 256
#define DIRECT_INDEX_NUMS 100
#define SLAB_NUMS	1000


#define SIZE_DIRECT_UNIT		(CHUNK_SIZE)
#define SIZE_FIRST_DIRECT_UNIT 	(CHUNK_SIZE)
#define SIZE_SECOND_DIRECT_UNIT (CHUNK_SIZE * INDIRECT_INDEX_NUMS)
#define SIZE_THIRD_DIRECT_UNIT 	(CHUNK_SIZE * INDIRECT_INDEX_NUMS * INDIRECT_INDEX_NUMS)

#define SIZE_DIRECT 		(DIRECT_INDEX_NUMS * CHUNK_SIZE)
#define SIZE_FIRST_DIRECT	(INDIRECT_INDEX_NUMS * CHUNK_SIZE)
#define SIZE_SECOND_DIRECT 	(INDIRECT_INDEX_NUMS * INDIRECT_INDEX_NUMS * CHUNK_SIZE )
#define SIZE_THIRD_DIRECT 	(INDIRECT_INDEX_NUMS * INDIRECT_INDEX_NUMS * INDIRECT_INDEX_NUMS * CHUNK_SIZE)


#define SIZE_DIRECT_END 		( SIZE_DIRECT )
#define SIZE_FIRST_DIRECT_END 	( (SIZE_DIRECT_END)			+ (SIZE_FIRST_DIRECT))
#define SIZE_SECOND_DIRECT_END 	( (SIZE_FIRST_DIRECT_END) 	+ (SIZE_SECOND_DIRECT))
#define SIZE_THIRD_DIRECT_END 	( (SIZE_SECOND_DIRECT_END)	+ (SIZE_THIRD_DIRECT))


/*
	注意在结构体内部使用结构体类型时需要加上struct
*/
typedef struct context{
	int chunk_index;
	size_t size;
	struct context * next;
	//int *backups;					// fuse_back3.c
	void ** which_inode_it_is;		// 会被强制转化为icontext**类型
}context;

typedef struct  indirect_class1
{
	context * addr[INDIRECT_INDEX_NUMS];
	int level;
}indirect_class1;


typedef struct  indirect_class2
{
	indirect_class1 * addr[INDIRECT_INDEX_NUMS];
	int level;
}indirect_class2;


typedef struct  indirect_class3
{
	indirect_class2 * addr[INDIRECT_INDEX_NUMS];
	int level;
}indirect_class3;

// 要注意，使用间接寻址的话删除的时候是比较麻烦的，你可能还是需要维护一个链表让他删除的时候快一点


typedef struct  file_context_index
{
	context * addr[DIRECT_INDEX_NUMS];
	indirect_class1 * in1addr;
	indirect_class2 * in2addr;
	indirect_class3 * in3addr;
}file_index;


typedef struct inode{ 
	char filename[FILE_NAME_LEN];
	size_t size;	// 实际写入大小

	size_t space;	// 实际占用的大小

	time_t timeLastModified;
	char isDirectories;

	int son_nums;
	pthread_mutex_t file_lock;
	file_index cnt_id;
	
	struct list_head list;		// 当inode处于空闲链表时有用
	HashMap * d_hash;			// 当inode是目录时这一项有用

	struct{
		struct context * context;
	};

	struct {
		struct inode * son;		// son指针指向其自身第一个儿子节点，父节点的son指向其所有d_bro子节点的头节点
		struct inode * bro;
	};

}inode;


typedef struct inode_list{
	struct inode_list * next;
	char isDirectories;
	char filename[FILE_NAME_LEN];
}inode_list;



struct attr {
	int size;
	time_t timeLastModified;
	char isDirectories;
};







// 全局变量，非线程安全
#ifdef DHMP_ON
	#define SSERVERNUM (SERVERNUM+1)
	#define CACHE_SIZE (CACHE_SIZE_BYTE / BANK_SIZE)

	
	typedef struct  rw_task
	{
		int bank_id;
		size_t size;
		off_t bank_offset;
		void * data;
		struct list_head    list;
	}rw_task;

	enum fuse_server_state{
		CONNECTED,
		DISCONNECT,
		RECOVERING,
		RECONNECT
	};

	typedef struct server_node{
		struct list_head list;
		int server_id;
		int alive;
		void * bank[BANK_NUM]; 
		void * test_alive_addr;
		char ip_addr[DHMP_ADDR_LEN];

		rw_task * rw_task_head;		// 重写缓冲区链表
		int rw_tasks_len;	// 重写缓冲区链表长度

		enum fuse_server_state server_state;
		pthread_mutex_t server_mutex;	// 修改锁


	}server_node;

	extern FILE * fuse_journal_fp; // 文件系统写入日志（写和删除）


	typedef struct  cache_DRAM
	{
		void * data;
		int bank_Id;
		pthread_mutex_t rewrite_lock;	// 脏页回写锁
		struct  list_head list;
	}cache_DRAM;


	typedef struct bank_info
	{
		int bank_id;
		int backup_servers[3];
	}bank_info;


	typedef struct dhmp_fs_cahce
	{
		int bank_id;
		cache_DRAM * cache_ptr;
	}dhmp_fs_cahce;
	
	
	
#endif

extern inode *root;         // 根节点
extern char bitmap[CHUNK_NUM];
extern context * context_addr[CHUNK_NUM]; 	// 提前为所有的context分配地址,一个context的空间就是一个chunk

#ifdef SSD_TEST
	extern off_t bank[BANK_NUM];      // bank[init_bank_i] = 文件偏移量;
	extern int _SSD;					// 模拟内存的文件
	extern pthread_mutex_t _SSD_LOCK;		// 文件操作锁
#else
	extern void * bank[BANK_NUM];      // bank[init_bank_i] = dhmp_malloc(BANK_SIZE,0);
#endif


extern FILE * fp;
extern char msg[1024];
extern char msg_tmp[1024];
extern double read_total_time;
extern pthread_mutex_t context_lock;


// inode slab和其对应的锁
extern inode * free_inode_slab;
extern pthread_mutex_t inode_slab_lock;



// 全局变量，非线程安全
#ifdef DHMP_ON
	extern int BACKUPNUMS;
	extern int SERVERNUM;
	extern int read_server_id;
	extern server_node ** fuse_sList;
	extern char * alive_context;

	extern FILE * fuse_journal_fp; // 文件系统写入日志（写和删除）
	extern pthread_mutex_t journal_mutex;	// 日志锁
	extern pthread_mutex_t fuse_mutex;  // 文件系统同步锁
	extern pthread_mutex_t recovery;
	extern pthread_rwlock_t cache_lock;
	extern pthread_mutex_t dirty_bitmap_lock;	// dirty_bitmap_bank的锁

	extern bool recovery_in_use;
	extern void * dram_bank[BANK_NUM];      // bank[init_bank_i] = dhmp_malloc(BANK_SIZE,0);
	extern int dirty_bitmap[BANK_NUM];	// 
	extern int fuse_journal_len;	// 日志缓冲长度
	extern int use_bank_len;
	extern rw_task * fuse_journal_list;	// 日志缓冲
	extern cache_DRAM * cache_head;		// cahce链表
	// extern HashMap * cache_hash;		// cache散列表
	extern dhmp_fs_cahce* cache_hash[CACHE_SIZE];
	extern int all_while_true_thread_kill;		// 当主进程结束的时候设置为true
#endif


#endif