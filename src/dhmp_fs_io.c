#include "../include/dhmp_fs.h"
#include "../include/dhmp_fs_logs.h"
#include "../include/dhmp_fs_io.h"
#include "../include/dhmp_fs_index.h"
#include "../include/dhmp_fs_dir_ops.h"
#include "../include/dhmp_fs_recovery.h"

static void dhmp_fs_read_from_remote( int bank_id,  char * buf, size_t size, off_t offset);
static void dhmp_fs_write_to_remote(int bank_id,  char * buf, size_t size, off_t offset);
static void dhmp_fs_read_from_cache( int bank_id,  char * buf, size_t size, off_t offset);
static void dhmp_fs_write_to_cache(int bank_id, char * buf, size_t size, off_t offset);
#ifdef DHMP_ON
void* write_all_pthreads(void *aargs);
void* write_pthread(void *aargs);
void * rewrite_dirty_cache();

//#define TIME
//#define DDBUG


// 一共只有100个块，遍历一遍是非常快的
// cache_head读者，dirty_bitmap写者
int try_visit_cache(int bank_id, char* local_buffer, size_t size, off_t offset, int type)
{ 
	// 先读缓存
	
	int count = 0;
	int cache_key = bank_id & (CACHE_SIZE -1);

	// pthread_rwlock_rdlock(&cache_lock);		// 以读者模式加锁
	// list_for_each_entry(cache, &cache_head->list, list)
	if(cache_hash[cache_key]->bank_id != bank_id)
	{	// miss!
		// pthread_rwlock_unlock(&cache_lock);
		return 0;
	}else
	{
		cache_DRAM * cache = cache_hash[cache_key]->cache_ptr;
		if(type == 1)
		{
			memcpy(local_buffer, cache->data + offset, size);		// 读缓存
		}
		else if(type == 2)
		{
			pthread_mutex_lock(&cache->rewrite_lock);
			memcpy(cache->data + offset, local_buffer, size);		// 写缓存
			cache->dirty = 1;										// 将缓存标记为脏
			pthread_mutex_unlock(&cache->rewrite_lock);
		}
		// pthread_rwlock_unlock(&cache_lock);
		return 1;
	}
}

void * rewrite_dirty_cache()
{
	pthread_detach(pthread_self());
	static int count = 0;
	while(true)
	{
		usleep(1000);
		int i=0;
		cache_DRAM * cache = NULL;
		// 遍历CACHE_SIZE链表
		i = 0;
		for(i =0 ; i<CACHE_SIZE; i++)
		{
			if(cache_hash[i]->cache_ptr != NULL)
			{
				cache = cache_hash[i]->cache_ptr;
				if(cache->dirty == 1)
				{
					if(pthread_mutex_trylock(&cache->rewrite_lock) == 0){
						dhmp_fs_write_to_remote(cache->bank_Id, cache->data, BANK_SIZE, 0);			// 把逐出的cache回写
						cache->dirty = 0;
						pthread_mutex_unlock(&cache->rewrite_lock);
					}else{
						continue;
					}
				}
			}
		}
		// FUSE_INFO_LOG("rewrite_dirty_cache %d!", ++count);
	}
}

// 将队尾cache逐出并回写
void expel_cache_rewrite(int new_bank_id)
{
	cache_DRAM * cache = cache_hash[ new_bank_id & (CACHE_SIZE - 1)]->cache_ptr;
	static int cache_miss_time = 0;
#ifdef DDBUG
	FUSE_INFO_LOG("cache_miss_time is %d", ++cache_miss_time);
#endif
	// pthread_rwlock_wrlock(&cache_lock);	
	if(cache->bank_Id == new_bank_id)
	{
		// pthread_rwlock_unlock(&cache_lock);	
		return;
	}
	
	// cache = list_prev_entry(cache_head, list);									// 获得队尾节点，将其逐出，写者（读的目的是为了写，那么都算写者）
	
	pthread_mutex_lock(&cache->rewrite_lock);
	if(cache->dirty == 1)
	{
		dhmp_fs_write_to_remote(cache->bank_Id, cache->data, BANK_SIZE, 0);		// 把逐出的cache回写
	}

	cache_hash[ cache->bank_Id & (CACHE_SIZE - 1)]->bank_id = new_bank_id;		// 将被逐出的bank在hash表的位置变化为新的bank
	cache->bank_Id = new_bank_id;												// 将该块缓存身份变更
	
	// list_move(&cache->list, &cache_head->list);									// 将其移动到队列头，再将其放入到队列头之后，其不可能被其他的线程当作逐出的对象

	dhmp_fs_read_from_remote(new_bank_id, cache->data, BANK_SIZE, 0);	

	cache->dirty = 0;

	pthread_mutex_unlock(&cache->rewrite_lock);	
	// pthread_rwlock_unlock(&cache_lock);											// 此时就可以解锁了，fs只负责保证cache_list的并发安全性，不保证写入和读取的并发安全性
}

typedef struct naive_rewrite_thread_arg
{
	int bank_id;
}naive_rewrite_thread_arg;


// 新写者进程，这是唯一的写者进程
static long int miss_count;
void* naive_rewrite_thread(void * arg)
{
	// 开辟线程去回写和更新缓存
#ifdef DDBUG
	FUSE_INFO_LOG("CACHE: miss count is %d", ++miss_count);
#endif
	pthread_detach(pthread_self());						// 父线程不等待子线程，子线程结束后释放自己的资源
	cache_DRAM * cache = NULL;
	int bank_id = ((naive_rewrite_thread_arg *) arg)->bank_id;

	pthread_rwlock_wrlock(&cache_lock);					// 以写者模式加锁
	// 进入写者执行范围
	cache = list_prev_entry(cache_head, list);			// 获得队尾节点，写者（读的目的是为了写，那么都算写者）

	cache_hash[ cache->bank_Id & (CACHE_SIZE - 1)]->bank_id = bank_id;		// 将被逐出的bank在hash表的位置变化为新的bank

	cache->bank_Id = bank_id;							// 将该块缓存身份变更
	list_move(&cache->list, &cache_head->list);			// 将其移动到队列头，再将其放入到队列头之后，其不可能被其他的线程当作逐出的对象


	// 回写
	dhmp_fs_write_to_remote(cache->bank_Id, cache->data, BANK_SIZE, 0);
	// 更新缓存
	dhmp_fs_read_from_remote(bank_id, cache->data, BANK_SIZE, 0);
	pthread_rwlock_unlock(&cache_lock);

	free(arg);
	return NULL;
}


	// pthread_t thread;
	// writeThreadArgs2 * Args = (writeThreadArgs2 *)malloc(sizeof(writeThreadArgs2));
	// cache = list_prev_entry(cache_head, list);	// 获得队尾节点
	// Args->bank_id = cache->bank_Id;
	// Args->buf = cache->data;
	// Args->offset = 0;
	// Args->size = BANK_SIZE;
	// pthread_create(&thread, NULL, write_all_pthreads, (void*)Args);

#endif


void cal_time(struct timespec * start, struct timespec *end, const char * commit)
{
#ifdef TIME
    unsigned long long total_write_time = ((end->tv_sec * 1000000000) + end->tv_nsec) -
            ((start->tv_sec * 1000000000) + start->tv_nsec); 
    FUSE_INFO_LOG("FUSE %s is %lf ns", commit, (double)(total_write_time));
#endif
}

void cal_log_time(struct timespec * start, struct timespec *end, int times)
{
#ifdef TIME
    unsigned long long total_write_time = ((end->tv_sec * 1000000000) + end->tv_nsec) -
            ((start->tv_sec * 1000000000) + start->tv_nsec); 
    FUSE_INFO_LOG("FUSE log time is %lf ns\n", times *(double)(total_write_time));
#endif
}


int dhmp_fs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	static int total_write_count = 0;
	struct timespec start_test, end_test;
	struct timespec strat_l3, end_l3;
    unsigned long long total_write_time = 0, total_fsync_time = 0; 
#ifdef DDBUG
	FUSE_INFO_LOG("Total write count is %d", ++total_write_count);
	FUSE_INFO_LOG("Write Begin! path %s, APP set write size is : %d,  now file offset is %d, wirte count is %d", path, size, offset, size/CHUNK_SIZE);
#endif
	int res;
	inode * head = (inode *)fi->fh;		// 现在可以直接从fi中获取文件的inode

	if(head == NULL || head->isDirectories == 1){
		FUSE_ERROR_LOG("ERROR ! Write to dirtory!,  return =-1");
		return -1;
	}

	size_t write_size = 0,	un_write_size = size;
	int write_count = 0;
	clock_gettime(CLOCK_MONOTONIC, &start_test);
	while(un_write_size > 0)
	{
		off_t chunk_offset = offset;
		off_t * offset_ptr = &chunk_offset;


		clock_gettime(CLOCK_MONOTONIC, &strat_l3);
		context * cnt =  level3_local_index(offset_ptr, head, 0);
		clock_gettime(CLOCK_MONOTONIC, &end_l3);
		cal_time(&strat_l3, &end_l3, "level chunk time");


		#ifdef DDBUG
		FUSE_INFO_LOG("After level3_local_index, chunk offset is %ld, file offset is %ld", chunk_offset, offset);
		#endif

		clock_gettime(CLOCK_MONOTONIC, &strat_l3);
		int bank_id = (cnt->chunk_index * (CHUNK_SIZE)) / (BANK_SIZE);
		off_t bank_offset = (cnt->chunk_index * (CHUNK_SIZE)) % (BANK_SIZE);
		
		if(un_write_size >= (CHUNK_SIZE - chunk_offset)){

			if(CHUNK_SIZE - chunk_offset <= 0){
				FUSE_ERROR_LOG("ERROR !  CHUNK_SIZE - chunk_offset <= 0 !");
			}

			// 将当前chunk写满
			#ifdef DDBUG
			FUSE_INFO_LOG("now index is %d,bank id is %d, wirte size is %lu, bank_offset is %ld, b + c = %ld", cnt->chunk_index, bank_id, (CHUNK_SIZE - chunk_offset), bank_offset,bank_offset+chunk_offset);
			#endif
			dhmp_fs_write_to_cache(bank_id, (char*)(buf + write_size), (CHUNK_SIZE - chunk_offset), bank_offset+chunk_offset);

			size_t now_write_size =  (size_t)(CHUNK_SIZE - chunk_offset);

			write_size += now_write_size;
			un_write_size -= now_write_size;

			offset += now_write_size;		// 文件偏移量 += 写入的长度

			pthread_mutex_lock(&head->file_lock);
			head->size += (size_t)(CHUNK_SIZE - cnt->size);
			cnt->size = CHUNK_SIZE;
			pthread_mutex_unlock(&head->file_lock);
		}
		else
		{
			// 将剩下的未写入数据都写进去
			#ifdef DDBUG
			FUSE_INFO_LOG("Final write now index is %d, bank id is %d, wirte size(un_write_size) is %lu, bank_offset is %ld, b + c = %ld", cnt->chunk_index, bank_id, un_write_size, bank_offset, bank_offset+chunk_offset);
			#endif
			dhmp_fs_write_to_cache(bank_id, (char*)(buf + write_size) , un_write_size, bank_offset+chunk_offset);
			write_size += un_write_size;
			size_t end_offset = un_write_size + chunk_offset;
			if( end_offset > cnt->size )
			{
				pthread_mutex_lock(&head->file_lock);
				if(cnt->size < end_offset){
					head->size += (end_offset - cnt->size);
					cnt->size = end_offset;
				}
				pthread_mutex_unlock(&head->file_lock);
			}
			un_write_size = 0;
		}
		clock_gettime(CLOCK_MONOTONIC, &end_l3);
		cal_time(&strat_l3, &end_l3, "write chunk time");
		write_count++;
	}
	clock_gettime(CLOCK_MONOTONIC, &end_test);
	cal_time(&start_test, &end_test, "write total time");


	clock_gettime(CLOCK_MONOTONIC, &start_test);
#ifdef DDBUG
	FUSE_INFO_LOG("write sucess!, Now filse size is: %d, Now offser is %ld, wirte count is %d\n", head->size, offset + write_size, write_count);
#endif
	clock_gettime(CLOCK_MONOTONIC, &end_test);
	cal_log_time(&start_test, &end_test, (write_count-1)*2 + 2);

	return size;
}

int dhmp_fs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{	
	static int total_read_count = 0;
	struct timespec start_test, end_test;
	struct timespec strat_l3, end_l3;
    unsigned long long total_write_time = 0, total_fsync_time = 0; 
#ifdef DDBUG
	FUSE_INFO_LOG("Total read count is %d", ++total_read_count);
	FUSE_INFO_LOG("READ Begin! path %s, APP set read size is : %d,  now file offset is %d, loop count is %d", path, size, offset, size/CHUNK_SIZE);
#endif
	

	inode * head = (inode *)fi->fh;		// 现在可以直接从fi中获取文件的inode

	// FUSE_INFO_LOG("File size is %lu", head->size);
	if(head == NULL || head->isDirectories == 1){
		FUSE_ERROR_LOG("ERROR ! read a dirtory!,  return = -1");
		return -1; // 如果filename不存在或者是目录返回-1
	}
	
	size_t read_size = 0,	un_read_size = size;   // read_size已经读了的长度，un_read_size还未读的长度
	int read_count = 0;
	clock_gettime(CLOCK_MONOTONIC, &start_test);
	while(un_read_size > 0)
	{
		// FUSE_INFO_LOG("read_size=%lu, un_read_size=%lu", read_size, un_read_size);
		off_t chunk_offset = offset;
		off_t * offset_ptr = &chunk_offset;

		clock_gettime(CLOCK_MONOTONIC, &strat_l3);
		context * cnt =  level3_local_index(offset_ptr, head, 1);
		clock_gettime(CLOCK_MONOTONIC, &end_l3);
		cal_time(&strat_l3, &end_l3, "lev3 chunk time");

		clock_gettime(CLOCK_MONOTONIC, &strat_l3);
		if(cnt == NULL){
			FUSE_INFO_LOG("cnt == NULL, Break!");
			break;
		}
		#ifdef DDBUG
		FUSE_INFO_LOG("After level3_local_index, cnt->size  %lu,  chunk offset is %ld,  file offset is %ld, un_read_size is %lu", cnt->size, chunk_offset, offset, un_read_size);
		#endif
		int bank_id = (cnt->chunk_index * (CHUNK_SIZE)) / (BANK_SIZE);
		off_t bank_offset = (cnt->chunk_index * (CHUNK_SIZE)) % (BANK_SIZE);
		if(un_read_size >= (cnt->size - chunk_offset)){    // 如果需要读的长度超过起始chunk偏移量之后到chunk结束剩下的长度

            if(cnt->size - chunk_offset < 0){
				FUSE_ERROR_LOG("ERROR !  cnt->size  %lu < chunk_offset %ld", cnt->size, chunk_offset);
				break;
			}

			size_t now_read_size = (size_t)(cnt->size  - chunk_offset);
			if(now_read_size == 0){
				#ifdef DDBUG
				FUSE_INFO_LOG("Now_read_Size == 0, break;");
				#endif
				break;
			}
			#ifdef DDBUG
			FUSE_INFO_LOG("now index is %d,   bank id is %d,  now_read_size is %lu,  bank_offset is %ld, b + c = %ld", cnt->chunk_index, bank_id, now_read_size, bank_offset, bank_offset+chunk_offset);
			#endif
			dhmp_fs_read_from_cache(bank_id, (char*)(buf + read_size), now_read_size, bank_offset+chunk_offset);

			read_size += now_read_size;
			un_read_size -= now_read_size;
			offset += now_read_size;		// 文件偏移量 += 读入的长度
		}
		else
		{   // 把剩下的都读出来
			#ifdef DDBUG
			FUSE_INFO_LOG("Final Read: now index is %d,bank id is %d, read size(un_read_size) is %lu, bank_offset is %ld, b + c = %ld", cnt->chunk_index, bank_id, un_read_size, bank_offset+chunk_offset);
			#endif
			dhmp_fs_read_from_cache(bank_id , (char*)(buf + read_size) , un_read_size, bank_offset+chunk_offset);
			read_size += un_read_size;
			un_read_size = 0;
			
		}
		clock_gettime(CLOCK_MONOTONIC, &end_l3);
		cal_time(&strat_l3, &end_l3, "read chunk time");
		read_count++;
	}
	clock_gettime(CLOCK_MONOTONIC, &end_test);
	cal_time(&start_test, &end_test, "read total time");

	clock_gettime(CLOCK_MONOTONIC, &start_test);
#ifdef DDBUG
	FUSE_INFO_LOG("READ DONE! APP set read size is : %d,  now file offset is %d, actulay read size is %d, loop count is %d\n", size, offset + read_size, read_size, read_count);
#endif
	clock_gettime(CLOCK_MONOTONIC, &end_test);
	cal_log_time(&start_test, &end_test, (read_count-1)*2 + 2);
	// buf[read_size] = 0;
	return read_size;
}




// 缓存不命中开销会非常大：一次写BANK_SIZE远端，一次读BANK_SIZE远端，一次memcpy
// 其实可以优化，因为这两次读和写一定不是同一块内存地址，所以可以并发进行？但是buf只有一块啊，不行
// 还是维护一个可用的buffer队列吧，进行memcpy的时间肯定比访问远端快得多
static void dhmp_fs_read_from_cache( int bank_id,  char * buf, size_t size, off_t offset)
{

	//FUSE_INFO_LOG("dhmp_fs_read_from_cache : bank_id is %d, read size is %lu, bank_offset is %ld", bank_id, size, offset);
#if DHMP_ON

	// 以读者身份读缓存，会被阻塞在写者进程上
#ifdef CACHE_ON
	if(try_visit_cache(bank_id, buf, size, offset, 1) == 1){
		return;
	}else{
		expel_cache_rewrite(bank_id);
		try_visit_cache(bank_id, buf, size, offset, 1);
	}
#else
	dhmp_fs_read_from_remote(bank_id, buf, size, offset);
#endif
	// // 开辟写者进程去回写数据和更新缓存
	// pthread_t thread;
	// naive_rewrite_thread_arg * arg = (naive_rewrite_thread_arg *) malloc(sizeof(naive_rewrite_thread_arg));
	// arg->bank_id = bank_id;
	// pthread_create(&thread, NULL, naive_rewrite_thread, (void*)arg);


#elif SSD_TEST
	pthread_mutex_lock(&_SSD_LOCK);
	off_t tbank = bank[bank_id];
	pread(_SSD, buf, size, tbank + offset);
	pthread_mutex_unlock(&_SSD_LOCK);
#else
	void * tbank = (bank[bank_id]);
	memcpy(buf, tbank + offset, size);
	return;
#endif
}


void dhmp_fs_write_to_cache(int bank_id, char * buf, size_t size, off_t offset)
{
	//FUSE_INFO_LOG("dhmp_fs_write_to_cache : bank_id is %d, read size is %lu, bank_offset is %ld", bank_id, size, offset);

		// CHUNK_NUM/BANK_NUM 平均一个bank里面有多少个chunk
#if DHMP_ON
	// 以读者身份读缓存，会被阻塞在写者进程上
#ifdef CACHE_ON
	if(try_visit_cache(bank_id, buf, size, offset, 2) == 1){
		return;
	}else{
		expel_cache_rewrite(bank_id);
		try_visit_cache(bank_id, buf, size, offset, 1);
	}
#else
	dhmp_fs_write_to_remote(bank_id, buf, size, offset);
#endif
	// // 开辟写者进程去回写数据和更新缓存
	// pthread_t thread;
	// naive_rewrite_thread_arg * arg = (naive_rewrite_thread_arg *) malloc(sizeof(naive_rewrite_thread_arg));
	// arg->bank_id = bank_id;
	// pthread_create(&thread, NULL, naive_rewrite_thread, (void*)arg);


	// 写日志缓冲区
	// if(SERVERNUM <= 2){
	// 	rw_task * wtask = (rw_task*) malloc(sizeof(rw_task));
	// 	wtask->bank_id = bank_id;
	// 	wtask->bank_offset = offset;
	// 	wtask->size = size;
	// 	list_add_tail(&wtask->list, &fuse_journal_list->list);
	// 	fuse_journal_len++;

	// 	if(!dirty_bitmap_bank[bank_id]){
	// 		use_bank_len++;
	// 		dirty_bitmap_bank[bank_id] = 1;
	// 	}
	// 	if(use_bank_len == BANK_NUM){
	// 		// 如果use_bank_len == BANK_NUM日志缓冲区没有必要继续存在了
	// 	}
	// }
	//FUSE_INFO_LOG("CACHE: dhmp_fs_write_to_bank over!");
	return;

#elif SSD_TEST
	pthread_mutex_lock(&_SSD_LOCK);
	off_t tbank = bank[bank_id];
	pwrite(_SSD, buf, size, tbank + offset);
	pthread_mutex_unlock(&_SSD_LOCK);
#else
	void * tbank = (bank[bank_id]);  // 获得bank的index（需要跳过多少个bank）
	memcpy(tbank + offset, buf, size);
	return ;

#endif
}


#ifdef DHMP_ON
void* write_pthread(void *aargs){
	writeThreadArgs *args = (writeThreadArgs *)aargs;
	void * tbank=args->tbank;
	char * buf=args->buf;
	off_t offset=args->offset;
	size_t size=args->size;
	////FUSE_INFO_LOG("write_pthread write server %d begin", args->serverId);
	args->re = dhmp_write(tbank, buf, size, offset);		// 值结果参数
	////FUSE_INFO_LOG("write_pthread write server %d  sucess", args->serverId);
	return NULL;
}

void* write_all_pthreads(void *aargs){
	writeThreadArgs2 *args = (writeThreadArgs2 *)aargs;
	dhmp_fs_write_to_remote(args->bank_id, args->buf, args->size, args->offset);
	return NULL;
}


// void dhmp_fs_write_to_bank(int chunk_index, char * buf, size_t size, off_t chunk_offset, bool cache_expel)
void dhmp_fs_write_to_remote(int bank_id,  char * buf, size_t size, off_t offset)
{
// 写远端服务器
	struct timespec start_test, end_test;
	// 计时开始
	//clock_gettime(CLOCK_MONOTONIC, &start_test);
	int i=0;

	// if(pthread_mutex_lock(&fuse_mutex)==0){

		//clock_gettime(CLOCK_MONOTONIC, &end_test); 
		// 计时结束
		// unsigned long long total_write_time = ((end_test.tv_sec * 1000000000) + end_test.tv_nsec) -
		// 			((start_test.tv_sec * 1000000000) + start_test.tv_nsec); 
		// double total_time = (double)(total_write_time/1000000);
		// //FUSE_INFO_LOG("TEST: client has wait %lf ms", total_time);

		// 开辟线程写服务器
		pthread_t threads[SERVERNUM]; 
		writeThreadArgs ** all_args = (writeThreadArgs **) malloc(SERVERNUM * sizeof(writeThreadArgs *));
		// 实际发生写入的服务器编号
		int actual_write_servers[SERVERNUM];
		memset(actual_write_servers, -1, SERVERNUM);
		// try our best to backup
		for(i=0; i<SERVERNUM; i++){
			// random chose an alive server
			// need modify
			if(fuse_sList[i]->server_state == DISCONNECT ||  fuse_sList[i]->server_state == RECONNECT)
				continue;
			if(fuse_sList[i]->server_state == RECOVERING){
				// 此时服务器正在恢复，将此时的写入操作加入到恢复缓冲区
				//FUSE_INFO_LOG("server %d is RECOVERING, stop execute write.", fuse_sList[i]->server_id);
				rw_task * write_task = (rw_task*) malloc(sizeof(rw_task));
				write_task->bank_id = bank_id;
				write_task->bank_offset = offset;
				write_task->size = size;
				list_add_tail(&write_task->list, &fuse_sList[i]->rw_task_head->list);
				fuse_sList[i]->rw_tasks_len++;
				continue;
			}
			// 在执行写入过程中，不允许将该server作为恢复服务器

			// int server_lock = pthread_mutex_trylock(&fuse_sList[i]->server_mutex);
			int server_lock = 0;
			if(server_lock == 0){
				////FUSE_INFO_LOG("server %d Begin to write", fuse_sList[i]->server_id);
				actual_write_servers[i] = 1;
				void * tbank = (fuse_sList[i]->bank[bank_id]);  
				writeThreadArgs * Args = (writeThreadArgs *)malloc(sizeof(writeThreadArgs));
				all_args[i] = Args;
				Args->re = -2;
				Args->buf = buf;
				Args->offset = offset;
				Args->size = size;
				Args->tbank = tbank;
				Args->serverId = i;
				Args->bank_id = bank_id;
				pthread_create(&threads[i], NULL, write_pthread, (void*)Args);
			}else{
				FUSE_ERROR_LOG("server %d is locked! cat't write!", fuse_sList[i]->server_id);
			}
		}
		// count success backup nums;
		int sucess_count=0;
		for(i=0; i<SERVERNUM; i++){
			if(actual_write_servers[i] == 1){
				pthread_join(threads[i],NULL);
				if(all_args[i]->re == -1){
					/*写入失败，面临不一致问题
					将该写入任务加入到server重写缓冲区中*/
					fuse_sList[i]->alive = 0;
					fuse_sList[i]->server_state = DISCONNECT;
					FUSE_ERROR_LOG("server %d write FAIL!", fuse_sList[i]->server_id);
				}else if(all_args[i]->re != -2){
					sucess_count++;
					////FUSE_INFO_LOG("server %d write SUCCESS!", fuse_sList[i]->server_id);
				}else{
					//FUSE_INFO_LOG("WRITE ERROR!");
				}
				free(all_args[i]);
				// pthread_mutex_unlock(&fuse_sList[i]->server_mutex);
			}
		}
		free(all_args);
		if(sucess_count){
			// FUSE_journal_impl(fuse_journal_fp, "%d %d %d\n", chunk_index,  chunk_offset, size);
		}else{
			// 所有任务都写入失败，此时没有任何可用副本
			FUSE_ERROR_LOG("FATE ERROR! All write all fail!");
		}
	// 	pthread_mutex_unlock(&fuse_mutex);
	// }
}

static void dhmp_fs_read_from_remote( int bank_id,  char * buf, size_t size, off_t offset)
{
	// pthread_mutex_lock(&fuse_mutex);  // 获取文件系统全局修改锁：等待所有的write操作结束，之后暂停所有服务器的write操作
	server_node * read_server=NULL;
	read_server = fuse_sList[(int)rand()%SERVERNUM];
	while(read_server){
		if(read_server->server_state == RECONNECT || read_server->server_state == RECOVERING){
			// 处于RECOVERING， RECONNECT状态的机器不可读
			//FUSE_INFO_LOG("Server %d 's state is RECONNECT or RECOVERING, now change server to read", read_server->server_id);
			read_server = find_alive_server();
			if(!read_server){
				FUSE_ERROR_LOG("No server is Available, Read fail!");
				return;
			}
		}
		void * tbank = read_server->bank[bank_id];
		int re = dhmp_read(tbank, buf, size, offset);	// 直接把bank全部读到缓存里面
		if(re == -1){
			FUSE_ERROR_LOG("Server %d has down! It is starting to retry", read_server->server_id);
			read_server->alive = 0;
			read_server->server_state = DISCONNECT;
			read_server = find_alive_server();
		}
		else{
			// //FUSE_INFO_LOG("dhmp_read from server %d sucess!", read_server->server_id);
			return;
		}
	}
	FUSE_ERROR_LOG("No server alive!");
	//pthread_mutex_unlock(&fuse_mutex);  // 获取文件系统全局修改锁：等待所有的write操作结束，之后暂停所有服务器的write操作
	return;
}



		// pthread_t threads[CACHE_SIZE];
		// writeThreadArgs2 * args_list[CACHE_SIZE];
		// for(; i< CACHE_SIZE; i++){
		// 	args_list[i] = NULL;
		// }

	// 	list_for_each_entry(cache, &cache_head->list, list){
	// 		if(dirty_bitmap[i] == 1){
	// 			// pthread_mutex_lock(&cache->rewrite_lock);
	// 			// dhmp_fs_write_to_remote(cache->bank_Id, cache->data, BANK_SIZE, 0);
	// 			// dirty_bitmap[i] = 0;
	// 			// pthread_mutex_unlock(&cache->rewrite_lock);
	// 			writeThreadArgs2 * args = (writeThreadArgs2 *)malloc(sizeof(writeThreadArgs2));
	// 			args_list[i] = args;
	// 			args->bank_id = cache->bank_Id;
	// 			args->buf = cache->data;
	// 			args->offset = 0;
	// 			args->size = BANK_SIZE;
	// 			pthread_create(&threads[i], NULL, write_all_pthreads, (void*)args);
	// 		}
	// 		i++;
	// 	}
	// 	i = 0;
	// 	list_for_each_entry(cache, &cache_head->list, list){
	// 		if(args_list[i] != NULL){
	// 			pthread_join(&threads[i], NULL);

	// 			pthread_mutex_lock(&cache->rewrite_lock);
	// 			dirty_bitmap[args_list[i]->bank_id] = 0;		// 只有rewrite_dirty_cache才能将
	// 			pthread_mutex_unlock(&cache->rewrite_lock);
	// 		}
	// 		i++;
	// 	}
	// 	pthread_mutex_unlock(&dirty_bitmap_lock);
	// 	pthread_mutex_unlock(&cache_lock);	// 涉及到链表头的变化，所以需要对整个cache_list加锁
	// }



// // cache_head写者，dirty_bitmap写者
// cache_DRAM * get_clean_cache(int new_bank_id)
// {
// 	// 倒序遍历
// 	cache_DRAM * cache = NULL;
// 	int i = 0;
// 	pthread_mutex_lock(&cache_lock);			// 涉及到链表头的变化，所以需要对整个cache_list加锁
// 	list_for_each_entry_reverse(cache, &cache_head->list, list)
// 	{
// 		// 如果是干净的页面，可以直接覆盖，并将其标记为脏
// 		if(dirty_bitmap[i] == 0){
// 			dirty_bitmap[i] = 1;
// 			cache->bank_Id = new_bank_id;
// 			return cache;
// 		}
// 		i++;
// 	}
// 	pthread_mutex_unlock(&cache_lock);	// 涉及到链表头的变化，所以需要对整个cache_list加锁
// 	return NULL;
// }
#endif