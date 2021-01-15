#include "../include/dhmp_fs.h"
#include "../include/dhmp_fs_logs.h"
#include "../include/dhmp_fs_io.h"
#include "../include/dhmp_fs_index.h"
#include "../include/dhmp_fs_dir_ops.h"
#include "../include/dhmp_fs_recovery.h"


char bitmap[CHUNK_NUM];

context * context_addr[CHUNK_NUM]; 	// 提前为所有的context分配地址,一个context的空间就是一个chunk

#ifdef SSD_TEST
	off_t bank[BANK_NUM];      // bank[init_bank_i] = 文件偏移量;ss
#else
	void * bank[BANK_NUM];      // bank[init_bank_i] = dhmp_malloc(BANK_SIZE,0);
#endif

inode *root;         // 根节点
FILE * fp;
char msg[1024];
char msg_tmp[1024];
double read_total_time  = 0.0;


// inode slab和其对应的锁
inode * free_inode_slab;
pthread_mutex_t inode_slab_lock;

// context链表的锁
pthread_mutex_t context_lock;

// 模拟内存的文件
int _SSD;
// 文件操作锁
pthread_mutex_t _SSD_LOCK;


// 全局变量，非线程安全
#ifdef DHMP_ON
	int BACKUPNUMS=3;
	int SERVERNUM=0;
	int read_server_id=0;
	server_node ** fuse_sList;
	char * alive_context;

	FILE * fuse_journal_fp; 				// 文件系统写入日志（写和删除）
	pthread_mutex_t journal_mutex;			// 日志锁
	pthread_mutex_t fuse_mutex;  			// 文件系统同步锁
	pthread_mutex_t recovery;
	pthread_rwlock_t cache_lock;			// 读写锁：用于cache_head的加锁（读者写者问题）
	pthread_mutex_t dirty_bitmap_lock;		// dirty_bitmap_bank的锁
	
	pthread_t rewrite_dirty_cache_thread;	// 脏页回写进程

	bool recovery_in_use;
	void * dram_bank[BANK_NUM];      	// bank[init_bank_i] = dhmp_malloc(BANK_SIZE,0);
	int dirty_bitmap[BANK_NUM];			// 标记bank是否为脏，如果为脏则会被rewrite_dirty_cache回写到磁盘中
	int fuse_journal_len=0;				// 日志缓冲长度
	int use_bank_len = 0;
	rw_task * fuse_journal_list;		// 日志缓冲
	cache_DRAM * cache_head;			// cahce链表
	cache_DRAM * anon_cache_head;		// 匿名cahce链表，用于减少在发生cache miss时的malloc操作

	int all_while_true_thread_kill;		// 当主进程结束的时候设置为true
	
#endif






/*
Initialize filesystem
	 *
	 * The return value will passed in the `private_data` field of
	 * `struct fuse_context` to all file operations, and as a
	 * parameter to the destroy() method. It overrides the initial
	 * value provided to fuse_main() / fuse_new().
*/
static void*  dhmp_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
        int init_bank_i, server_i;
		FUSE_INFO_LOG("FUSE unit size：");
		FUSE_INFO_LOG("SIZE_DIRECT_UNIT :%lu", SIZE_DIRECT_UNIT);
		FUSE_INFO_LOG("SIZE_FIRST_DIRECT_UNIT :%lu", SIZE_FIRST_DIRECT_UNIT);
		FUSE_INFO_LOG("SIZE_SECOND_DIRECT_UNIT :%lu", SIZE_SECOND_DIRECT_UNIT);
		FUSE_INFO_LOG("SIZE_THIRD_DIRECT_UNIT :%lu", SIZE_THIRD_DIRECT_UNIT);

		FUSE_INFO_LOG("FUSE size：");
		FUSE_INFO_LOG("SIZE_DIRECT :%lu, %lfK",  SIZE_DIRECT, (double)SIZE_DIRECT / (1024));
		FUSE_INFO_LOG("SIZE_FIRST_DIRECT :%lu, %lf K", SIZE_FIRST_DIRECT, (double)SIZE_FIRST_DIRECT/ (1024));
		FUSE_INFO_LOG("SIZE_SECOND_DIRECT :%lu, %lf M",SIZE_SECOND_DIRECT, (double)SIZE_SECOND_DIRECT / (1024*1024));
		FUSE_INFO_LOG("SIZE_THIRD_DIRECT :%lu, %lf G", SIZE_THIRD_DIRECT, (double)SIZE_THIRD_DIRECT/ (1024*1024*1024));


		FUSE_INFO_LOG("FUSE accumulate size：");
		FUSE_INFO_LOG("SIZE_DIRECT_END :%lu, %lfK", SIZE_DIRECT_END, (double)SIZE_DIRECT_END / (1024));
		FUSE_INFO_LOG("SIZE_FIRST_DIRECT_END :%lu, %lf K",SIZE_FIRST_DIRECT_END,  (double)SIZE_FIRST_DIRECT_END/ (1024));
		FUSE_INFO_LOG("SIZE_SECOND_DIRECT_END :%lu, %lf M", SIZE_SECOND_DIRECT_END, (double)SIZE_SECOND_DIRECT_END / (1024*1024));
		FUSE_INFO_LOG("SIZE_THIRD_DIRECT_END :%lu, %lf G",SIZE_THIRD_DIRECT_END, (double) SIZE_THIRD_DIRECT_END/ (1024*1024*1024));


#if DHMP_ON
		recovery_in_use  = false;
		alive_context= malloc(CHUNK_SIZE);
		memset(alive_context, 0, CHUNK_SIZE);
		memset(dirty_bitmap, 0, BANK_NUM);


		dhmp_client_init(BANK_SIZE, fuse_recover);
		//FUSE_INFO_LOG("dhmp_client_init SUCESS!");
		SERVERNUM = dhmp_get_trans_count();
		if(SERVERNUM==0){
			FUSE_ERROR_LOG("SERVERNUM = 0");
		}
		else{
			if(SERVERNUM < BACKUPNUMS){
				BACKUPNUMS = SERVERNUM;
			}
			//FUSE_INFO_LOG("SERVERNUM = %d", SERVERNUM);
		}
		// 初始化服务端地址数组
		fuse_sList = (server_node **) malloc(SERVERNUM * sizeof(uintptr_t));

		// 初始化文件系统日志
		// fuse_journal_fp = fopen("/home/gtwang/FUSE/fuse_journal.txt", "a+");
		pthread_mutex_init(&journal_mutex, NULL);
		pthread_mutex_init(&fuse_mutex, NULL);
		pthread_mutex_init(&recovery, NULL);

		// 初始化日志缓冲区头节点
		fuse_journal_list = (rw_task*) malloc(sizeof(rw_task));
		INIT_LIST_HEAD(&fuse_journal_list->list);

		for(server_i=0; server_i< SERVERNUM; server_i++){
			// 分配文件系统的空间
			fuse_sList[server_i] = (server_node*)malloc(sizeof(server_node));
			fuse_sList[server_i]->server_id = server_i;
			fuse_sList[server_i]->alive = 1;
			fuse_sList[server_i]->test_alive_addr = dhmp_malloc(CHUNK_SIZE, server_i);	 // 给判活数组分配空间

			pthread_mutex_init(&fuse_sList[server_i]->server_mutex, NULL);

			// 初始化重写缓冲区链表
			fuse_sList[server_i]->rw_task_head = (rw_task*) malloc(sizeof(rw_task));
			INIT_LIST_HEAD(&fuse_sList[server_i]->rw_task_head->list);

			// 设置服务器状态
			fuse_sList[server_i]->server_state = CONNECTED;


			
			// 为bank分配内存
			for(init_bank_i = 0; init_bank_i < BANK_NUM; init_bank_i++){
				fuse_sList[server_i]->bank[init_bank_i] = dhmp_malloc(BANK_SIZE, server_i);
			}
			// 为DRAM_bank分配内存
			for(init_bank_i = 0;init_bank_i < BANK_NUM;init_bank_i++)
				dram_bank[init_bank_i] = malloc(BANK_SIZE);

			//FUSE_INFO_LOG("DHMP server %d malloc memory SUCCESS", server_i);
		}

		// 开启写脏页的线程
		// all_while_true_thread_kill = 0;			// 没什么鸟用，fuse启动后你的main函数就结束了

		// pthread_create(&rewrite_dirty_cache_thread, NULL, rewrite_dirty_cache, NULL);

		// 初始化cache链表
		// 先预存前20个bank
		pthread_rwlock_init(&cache_lock, NULL);			// 读写锁不光要初始化，还需要在不再使用后释放，但是在文件系统里面就算了

		cache_head = (cache_DRAM*) malloc(sizeof(cache_DRAM));
		INIT_LIST_HEAD(&cache_head->list);
		int i=0;
		for(; i< CACHE_SIZE; i++){
			cache_DRAM *cache = (cache_DRAM*) malloc(sizeof(cache_DRAM));
			cache->data = malloc(BANK_SIZE);
			memset(cache->data, 0, BANK_SIZE);
			cache->bank_Id = i;
			pthread_mutex_init(&cache->rewrite_lock, NULL);
			list_add_tail(&cache->list, &cache_head->list);
		}
		FUSE_INFO_LOG("Init_cache_DRAM  SUCCESS");

#elif SSD_TEST 
		// 初始化SSD磁盘
		_SSD = open("/home/gtwang/FUSE/FUSE_SSD.txt",  O_RDWR|O_CREAT, S_IRWXU);
		if(_SSD ==-1 ){
			FUSE_ERROR_LOG("Open file fail! %s\n", strerror(errno));
		}
		for(init_bank_i = 0;	init_bank_i < BANK_NUM;		init_bank_i++)
			bank[init_bank_i] = (off_t) (init_bank_i * BANK_SIZE);
		
		pthread_mutex_init(&_SSD_LOCK, NULL);
#else
		for(init_bank_i = 0;init_bank_i < BANK_NUM;init_bank_i++)
				bank[init_bank_i] = malloc(BANK_SIZE);
#endif	



	root = (inode*) malloc(sizeof(inode));
	root -> bro = NULL;		
	root -> son = NULL;			
	root -> isDirectories = 1;              // 是目录
	root -> size = 0;
	root -> timeLastModified = time(NULL);  
	memset(root->filename,0,FILE_NAME_LEN);
	root->filename[0] = '/';                		// 文件名为“/”
	memset(bitmap,0,sizeof(bitmap));        // 初始化所有的chunk为可用状态


	root -> d_hash = createHashMap(NULL, NULL);		// 初始化根目录的d_hash

	// INIT_LIST_HEAD(&root->d_son);					// 父节点的son指向是儿子的头节点		

	// need_change
	root ->context = (context * ) malloc(sizeof(context));
	root->context->next = NULL;
	root->son_nums = 0;



	// 初始化所有的context
	init_context();

	FUSE_INFO_LOG("init_context SUCCESS");

	// 初始化inode slab
	init_inode_slab();								
	FUSE_INFO_LOG("init_inode_slab SUCCESS");

	size_t size = BANK_SIZE;



#ifdef octopus
	dhmp_malloc(size,10); //for octo
#endif
#ifdef L5
	dhmp_malloc(size,3); //for L5
#endif
#ifdef FaRM
	dhmp_malloc(size,8); //for FaRM
#endif
#ifdef RFP
	dhmp_malloc(size,6); //RFP
#endif
#ifdef scaleRPC
	dhmp_malloc(size,7); //scaleRPC
#endif
#ifdef DaRPC
	dhmp_malloc(0,5); //DaRPC
#endif
}








// 首先需要定义文件系统支持的操作函数，填在结构体 struct fuse_operations 中
/*
	init程序：
		init进程，它是一个由内核启动的用户级进程。
		内核自行启动（已经被载入内存，开始运行，并已初始化所有的设备驱动程序和数据结构等）之后
		，就通过启动一个用户级程序init的方式，完成引导进程。
		所以,init始终是第一个进程（其进程编号始终为1）。
	gatattr函数：
	 	在通知索引节点需要从磁盘中更新时，VFS会调用该函数
	
	access函数：
		作用：检查是进入目录的权限
		描述：检查调用进程是否可以访问文件路径名。 如果路径名是符号链接，
			  则将其取消引用。该检查是使用调用进程的实际UID和GID完成的，
	
	readdir函数：
		readdir（）函数返回一个指向dirent结构的指针，
		该结构表示dirp指向的目录流中的下一个目录条目。 在到达目录流的末尾或发生错误时，它返回NULL。

	mknod函数：
		作用：创建文件
		描述：该函数被系统调用mknod()调用，创建特殊文件（设备文件，命名管道或套接字）。要创建的文件放在dir
			  目录中，其目录项为dentry，关联的设备为rdev，初始权限由mode指定。
	
	mkdir函数：
		创建文件夹
	
	unlink函数：
		删除文件
	
	rmdir函数：
		删除空的目录
	
	rename函数：
		重命名
	
	chmod函数：
		修改权限
	
	truncate函数
		truncate可以将一个文件缩小或者扩展到某个给定的大小

	open函数：
		打开文件描述符
	
	read函数：
		读取文件描述符
	
	write函数：
		写文件描述符
	
	statfs函数：
		主要用来获得磁盘的空间，外部执行df，df -i的时候，将调用.statfs进行状态查询


*/
static struct fuse_operations dhmp_fs_oper = {
	.init       = dhmp_init,
	.getattr	= dhmp_fs_getattr,
	.access		= dhmp_fs_access,
	.readdir	= dhmp_fs_readdir,
	.mknod		= dhmp_fs_mknod,
	.mkdir		= dhmp_fs_mkdir,
	.unlink		= dhmp_fs_unlink,
	.rmdir		= dhmp_fs_rmdir,
	.rename		= dhmp_fs_rename,
	.chmod		= dhmp_fs_chmod,
	.chown		= dhmp_fs_chown,
	.truncate	= dhmp_fs_truncate,
	.open		= dhmp_fs_open,
	.read		= dhmp_fs_read,
	.write		= dhmp_fs_write,
	.statfs		= dhmp_fs_statfs,
};

void dhmp_timer_test()
{

}

int main(int argc, char *argv[])
{
	printf("fuck fuse\n");
	// 用户态程序调用fuse_main() （lib/helper.c）
	int ret = fuse_main(argc, argv, &dhmp_fs_oper, NULL);
	printf("Bye!\n");

	// #ifdef DHMP_ON
	// int init_bank_i;
	// for(init_bank_i = 0;init_bank_i < BANK_NUM; init_bank_i++)
	// 	dhmp_free(bank[init_bank_i]);
	// dhmp_client_destroy();
	// #endif
	return ret;
}


