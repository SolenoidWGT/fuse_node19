#include "../include/dhmp_fs.h"
#include "../include/dhmp_fs_logs.h"
indirect_class1 * init_indirect_class1()
{
	indirect_class1 * it = (indirect_class1 *) malloc( sizeof(indirect_class1));
	int i=0;
	for(; i < INDIRECT_INDEX_NUMS; i++){
		it->addr[i] = NULL;
	}
	return it;
}


indirect_class2 * init_indirect_class2()
{
	indirect_class2 * it = (indirect_class2 *) malloc( sizeof(indirect_class2));
	int i=0;
	for(; i < INDIRECT_INDEX_NUMS; i++){
		it->addr[i] = init_indirect_class1();
	}
	return it;
}


indirect_class3 * init_indirect_class3()
{
	indirect_class3 * it = (indirect_class3 *) malloc( sizeof(indirect_class3));
	int i=0;
	for(; i < INDIRECT_INDEX_NUMS; i++){
		it->addr[i] = init_indirect_class2();
	}
	return it;
}

static long int malloc_times = 0;
inode* malloc_inode()
{
	// FUSE_INFO_LOG("malloc_inode enter!");
	malloc_times++;
	inode *  now = (inode * ) malloc(sizeof(inode));
	now -> isDirectories = 0;
	now -> size = 0;
	now -> timeLastModified = time(NULL);
	int j = 0;
	for(; j < DIRECT_INDEX_NUMS; j++){
		now->cnt_id.addr[j] = NULL;
	}
	now->cnt_id.in1addr = init_indirect_class1();
	now->cnt_id.in2addr = init_indirect_class2();			// 提前分配到二级索引
	now->cnt_id.in3addr = NULL;
	now->context = (context *) malloc(sizeof(context));		// 提前为该inode分配一个没有实际意义的context头节点
	now->context->next = NULL;
	memset(now->filename,0,FILE_NAME_LEN);
	pthread_mutex_init(&now ->file_lock, NULL);
	now->son_nums = 0;


	now->son = NULL;
	now->bro = NULL;
	// INIT_LIST_HEAD(&now->d_son);		// 父节点的son指向是遍历儿子节点时的头节点
	// FUSE_INFO_LOG("malloc_times is %ld", malloc_times);
	return now;
}

void init_inode_slab()
{
	int i = 0;
	free_inode_slab = (inode * ) malloc (sizeof(inode));
	INIT_LIST_HEAD(&free_inode_slab->list);
	for(i; i < SLAB_NUMS; i++){
		inode *  now = malloc_inode();
		list_add_tail(&now->list, &free_inode_slab->list);
	}
	pthread_mutex_init(&inode_slab_lock, NULL);					// inode slab和其对应的锁
}

void init_context()
{

	// 初始化所有的context
	int context_id = 0;
	for(; context_id < CHUNK_NUM; context_id++){

		context_addr[context_id] =  (context *)malloc(sizeof(struct context));
		context_addr[context_id]->chunk_index = context_id;
		context_addr[context_id]->size = 0;
		context_addr[context_id]->next = NULL;
		context_addr[context_id]->which_inode_it_is = NULL;
	}

	pthread_mutex_init(&context_lock, NULL);
}


void reset_indoe(inode * now)
{
	
	if(now == NULL){
		// FUSE_INFO_LOG("now == NULL! return !");
		return;
	}
	// FUSE_INFO_LOG("reset_indoe enter, inode name is %s, is dirct %d, size is %lu", now->filename, now -> isDirectories, now->size);

	if(now->isDirectories == 1)
	{
		now->d_hash->clear(now->d_hash);				// 如果是目录，需要重置dhash
		now->d_hash = NULL;
	}
		
	now -> isDirectories = 0;
	// now -> son = NULL;	// 不要把son置为NULL
	// now -> bro = NULL;		// bro没有用了
	now->son = NULL;
	now->bro = NULL;
	now -> size = 0;
	now -> space = 0;
	now -> timeLastModified = time(NULL);
	
	context * cnt = now->context->next, *tmp;			// 注意我们要跳过第一个context头节点
	now->context->next = NULL;
	int count = 0;
	
	while(cnt){
		tmp = cnt->next;
		cnt->next = NULL;
		*((context **)cnt->which_inode_it_is) = NULL;	// 将该指针所在的位置为NULL，表示删除
		cnt->which_inode_it_is= NULL;
		cnt->size = 0;
		bitmap[cnt->chunk_index] = 0;   				// 归还分配的磁盘空间
		cnt = tmp;

		count++;
	}

	// FUSE_INFO_LOG("while cnt is ol !");
	memset(now->filename,0,FILE_NAME_LEN);

	pthread_mutex_lock(&inode_slab_lock);
	list_add_tail(&now->list, &free_inode_slab->list);	// 将reset后的磁盘放置到空闲inode链表上
	pthread_mutex_unlock(&inode_slab_lock);

	// INIT_LIST_HEAD(&now->d_son);					// 重置d_son链表，初始化儿子节点的bro节点
	// FUSE_INFO_LOG("reset_indoe sucess!");
}




inode* get_free_inode()
{
	// FUSE_INFO_LOG("get_free_inode enter!");
	inode* free_inode = NULL;
	
	pthread_mutex_lock(&inode_slab_lock);
	if(list_empty_careful(&free_inode_slab->list) != 1)		// 这个函数是非线程安全的，需要加锁
	{
		free_inode = list_first_entry(&free_inode_slab->list, inode, list);	// list_entry返回值的是*type类型
		list_del(&free_inode->list);
	}
	pthread_mutex_unlock(&inode_slab_lock);
	if(free_inode == NULL){
		free_inode = malloc_inode();
	}
	// FUSE_INFO_LOG("get_free_inode sucess!");
	return free_inode;
}


context * get_free_context(){
	pthread_mutex_lock(&context_lock);
	int i = getFreeChunk();
	pthread_mutex_unlock(&context_lock);
	if(i == -1){
		return NULL;
	}
	return context_addr[i];
}

int getFreeChunk()
{
	int i =0;
	for(;i < CHUNK_NUM; i++)
	{
		if(bitmap[i] == 1)continue;
		break;
	}
	if(i == CHUNK_NUM){
		FUSE_ERROR_LOG("Error:no space for free chunk");
		return -1;
	}
	bitmap[i] = 1;
	return i;
}


void free_indirect_class1(inode * head)
{
	if(head->cnt_id.in1addr != NULL)
		free(head->cnt_id.in1addr);
	////FUSE_INFO_LOG("free_indirect_class1 sucess!");
}
void free_indirect_class2(inode * head)
{
	int i =0;
	if(head->cnt_id.in2addr  != NULL ){
		for(; i<INDIRECT_INDEX_NUMS; i++){
			free(head->cnt_id.in2addr->addr[i]);
		}
		free(head->cnt_id.in2addr);
	}
	////FUSE_INFO_LOG("free_indirect_class2 sucess!");
}
void free_indirect_class3(inode * head)
{
	int i =0;
	if(head->cnt_id.in3addr  != NULL){
		for(; i<INDIRECT_INDEX_NUMS; i++){
			int j =0;
			for(; j<INDIRECT_INDEX_NUMS; j++){
				free(head->cnt_id.in3addr->addr[i]->addr[j]);
			}
			free(head->cnt_id.in3addr->addr[i]);
		}
		free(head->cnt_id.in3addr);
	}
	////FUSE_INFO_LOG("free_indirect_class3 sucess!");
}


void insert_list(context * head, context * cnt)
{
	cnt->next = head->next;
	head->next = cnt;
}

context * level3_local_index(off_t* offset, inode* head, int only_read)
{
	// 寻找第一个被写入的chunk地址，这个过程必须加锁
	context * start_context = NULL;
	if((*offset) < SIZE_DIRECT_END ){
		// 直接寻址
		// 计算写入的chunk位置
		//FUSE_INFO_LOG("Direct_0 index");
		int in_0_id = (*offset) / (SIZE_DIRECT_UNIT);			// 零级索引位置
		(*offset) %= (SIZE_DIRECT_UNIT);						// 写入该chunk时的偏移量

		if(head->cnt_id.addr[in_0_id] == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.addr[in_0_id] == NULL){
				context * new_cnt  = get_free_context();
				head->cnt_id.addr[in_0_id] = new_cnt;
				new_cnt->which_inode_it_is = (void ** ) ( &head->cnt_id.addr[in_0_id] ) ;	// context * new_cnt的地址，在删除的时候方便修改
				insert_list(head->context, new_cnt);
			}
			pthread_mutex_unlock(&head->file_lock);
		}
		
		start_context = head->cnt_id.addr[in_0_id];
		//FUSE_INFO_LOG("in_0_id is %d, chunk_id is %d", in_0_id, start_context->chunk_index);

	}else if((*offset) < SIZE_FIRST_DIRECT_END){
		// 一次间接寻址
		// 如果in1addr没有初始化，先初始化in1addr
		//FUSE_INFO_LOG("indirect_1 index");
		if(head->cnt_id.in1addr == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.in1addr == NULL){
				head->cnt_id.in1addr = init_indirect_class1();
			}
			pthread_mutex_unlock(&head->file_lock);
		}
		

		// 计算写入的chunk位置
		(*offset) -=  (SIZE_DIRECT_END);						// 跳过所有的零级索引（直接索引）
		int in_1_id = (*offset) / (SIZE_FIRST_DIRECT_UNIT);	// 一级索引位置
		(*offset)  %= (SIZE_FIRST_DIRECT_UNIT);				// 写入该chunk时的偏移量

		// 如果对应一级索引位置位置没有context，先分配context
		
		if( head->cnt_id.in1addr->addr[in_1_id] == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.in1addr->addr[in_1_id] == NULL){
				context * new_cnt  = get_free_context();
				head->cnt_id.in1addr->addr[in_1_id] = new_cnt;
				new_cnt->which_inode_it_is = (void ** ) (&head->cnt_id.in1addr->addr[in_1_id]) ;	// context * new_cnt的地址，在删除的时候方便修改
				insert_list(head->context, new_cnt);
			}
			pthread_mutex_unlock(&head->file_lock);
		}
		
		start_context = (context *)head->cnt_id.in1addr->addr[in_1_id];

		//FUSE_INFO_LOG("in_1_id is %d, chunk_id is %d", in_1_id, start_context->chunk_index);

	}else if((*offset) < SIZE_SECOND_DIRECT_END){
		// 两次间接寻址
		// 如果in2addr没有初始化，先初始化in2addr
		//FUSE_INFO_LOG("indirect_2 index");

		if(head->cnt_id.in2addr == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.in2addr == NULL){
				head->cnt_id.in2addr = init_indirect_class2();
			}
			pthread_mutex_unlock(&head->file_lock);
		}

		// 计算写入的chunk位置
		(*offset) -= (SIZE_FIRST_DIRECT_END);					//	跳过所有的零级索引和一级索引

		int in_2_id = (*offset) / (SIZE_SECOND_DIRECT_UNIT);	//  二级索引位置
		(*offset) %= (SIZE_SECOND_DIRECT_UNIT) ;				//  二级索引的偏移量

		
		int in_1_id = (*offset) / (SIZE_FIRST_DIRECT_UNIT);	// 一级索引位置
		(*offset) %= (SIZE_FIRST_DIRECT_UNIT);					// 一级索引的偏移量

		if(head->cnt_id.in2addr->addr[in_2_id]->addr[in_1_id]  == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.in2addr->addr[in_2_id]->addr[in_1_id]  == NULL){
				context * new_context = get_free_context(head);
				head->cnt_id.in2addr->addr[in_2_id]->addr[in_1_id] = new_context;
				new_context->which_inode_it_is = (void ** ) (&head->cnt_id.in2addr->addr[in_2_id]->addr[in_1_id]);
				insert_list(head->context, new_context);
			}
			pthread_mutex_unlock(&head->file_lock);
		}

		start_context = head->cnt_id.in2addr->addr[in_2_id]->addr[in_1_id];
		
		//FUSE_INFO_LOG("in_2_id is %d, in_1_id is %d, chunk_id is %d", in_2_id, in_1_id, start_context->chunk_index);

	}else if((*offset) < SIZE_THIRD_DIRECT_END){
		// 三次间接寻址
		// 如果in3addr没有初始化，先初始化in3addr
		//FUSE_INFO_LOG("indirect_3 index");
		if(head->cnt_id.in3addr == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.in3addr == NULL){
				head->cnt_id.in3addr = init_indirect_class3();
			}
			pthread_mutex_unlock(&head->file_lock);
		}
		// 计算写入的chunk位置
		(*offset) -= (SIZE_SECOND_DIRECT_END);					//	跳过所有的零级索引，一级索引和二级索引

		int in_3_id = (*offset) / (SIZE_THIRD_DIRECT_UNIT);	//  三级索引位置
		(*offset) %= (SIZE_THIRD_DIRECT_UNIT) ;				//  三级索引的偏移量

		int in_2_id = (*offset) / (SIZE_SECOND_DIRECT_UNIT);	//  二级索引位置
		(*offset) %= (SIZE_SECOND_DIRECT_UNIT) ;				//  二级索引的偏移量

		int in_1_id = (*offset) / (SIZE_FIRST_DIRECT_UNIT);	//  一级索引位置
		(*offset) %= (SIZE_FIRST_DIRECT_UNIT) ;				//  一级索引的偏移量

		if(head->cnt_id.in3addr->addr[in_3_id]->addr[in_2_id]->addr[in_1_id]  == NULL){
			if(only_read == 1){
				return NULL;
			}
			pthread_mutex_lock(&head->file_lock);
			if(head->cnt_id.in3addr->addr[in_3_id]->addr[in_2_id]->addr[in_1_id]  == NULL){
				context * new_context = get_free_context(head);
				head->cnt_id.in3addr->addr[in_3_id]->addr[in_2_id]->addr[in_1_id] = new_context;
				new_context->which_inode_it_is = (void ** ) (&head->cnt_id.in3addr->addr[in_3_id]->addr[in_2_id]->addr[in_1_id]);
				insert_list(head->context, new_context);
			}
			pthread_mutex_unlock(&head->file_lock);
		}

		start_context = head->cnt_id.in3addr->addr[in_3_id]->addr[in_2_id]->addr[in_1_id];

		//FUSE_INFO_LOG("in_3_id is %d, in_2_id is %d, in_1_id is %d, chunk_id is %d", in_3_id, in_2_id, in_1_id, start_context->chunk_index);
	}else{
		// 超过文件系统最大文件大小限制
		FUSE_ERROR_LOG("Write ERROR! write offset is bigger than FS's largest size!");
	}
	return start_context;
}