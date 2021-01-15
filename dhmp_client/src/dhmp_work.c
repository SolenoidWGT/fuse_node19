#include "dhmp.h"
#include "dhmp_log.h"
#include "dhmp_hash.h"
#include "dhmp_config.h"
#include "dhmp_context.h"
#include "dhmp_dev.h"
#include "dhmp_transport.h"
#include "dhmp_task.h"
#include "dhmp_timerfd.h"
#include "dhmp_poll.h"
#include "dhmp_work.h"
#include "dhmp_watcher.h"
#include "dhmp_client.h"

// malloc_work->addr_info->nvm_mr.addr  ==  normal_addr
// 注意：nvm_mr.addr（NVM虚拟地址）是一个有效长度为48bit的指针，形如：0x7fcec9d7a000
void *dhmp_transfer_dhmp_addr(struct dhmp_transport *rdma_trans, void *normal_addr)
{
	long long node_index, ll_addr;	// 64bit = 16bit + 48bit
	int index;	// serverid有效长度为16bit
	void *dhmp_addr;
	
	for(index=0; index<DHMP_SERVER_NODE_NUM; index++)
	{
		if(rdma_trans==client->connect_trans[index])
			break;
	}
	DEBUG_LOG("nvm_mr.addr normal_addr == %p", normal_addr);
	node_index=index;	// 当前server的id

	if(node_index==0)
		return normal_addr;	 // 减少无用的操作
	
	node_index=node_index<<48;		
	ll_addr=(long long)normal_addr;
	ll_addr+=node_index;
	dhmp_addr=(void *)ll_addr;
	
	return dhmp_addr;
}

/**
 * dhmp_hash_in_client:cal the addr into hash key
 */
int dhmp_hash_in_client(void *addr)
{
	uint32_t key;
	int index;

	key=hash(&addr, sizeof(void*));	 	// MurmurHash3_x86_32
	/*key is a uint32_t value,through below fomula transfer*/
	index=((key%DHMP_CLIENT_HT_SIZE)+DHMP_CLIENT_HT_SIZE)%DHMP_CLIENT_HT_SIZE;

	return index;
}

/**
 *	dhmp_addr_info_insert_ht:
 *	add new addr info into the addr_info_hashtable in client
 */
static void dhmp_addr_info_insert_ht(void *dhmp_addr,
											struct dhmp_addr_info *addr_info)
{
	int index;

	addr_info->dram_mr.addr=NULL;
	index=dhmp_hash_in_client(dhmp_addr);	// 将serverID + NVM虚拟地址进行一致性hash
	DEBUG_LOG("insert ht %d %p",index,addr_info->nvm_mr.addr);
	hlist_add_head(&addr_info->addr_entry, &client->addr_info_ht[index]);
}

// 获得服务器节点id
int dhmp_get_node_index_from_addr(void *dhmp_addr)
{
	long long node_index=(long long)dhmp_addr;
	int res;
	node_index=node_index>>48;
	res=node_index;
	return res;
}

void dhmp_malloc_work_handler(struct dhmp_work *work)
{
	struct dhmp_malloc_work *malloc_work;
	struct dhmp_msg msg;
	struct dhmp_mc_request req_msg;
	void *res_addr=NULL;

	malloc_work=(struct dhmp_malloc_work*)work->work_data;
	
	/*build malloc request msg*/
	req_msg.req_size=malloc_work->length;
	req_msg.addr_info=malloc_work->addr_info;
	
	msg.msg_type=DHMP_MSG_MALLOC_REQUEST;
	msg.data_size=sizeof(struct dhmp_mc_request);
	msg.data=&req_msg;
	
	dhmp_post_send(malloc_work->rdma_trans, &msg);

	/*wait for the server return result*/
	while(malloc_work->addr_info->nvm_mr.length==0);
	
	res_addr=malloc_work->addr_info->nvm_mr.addr;
	
	DEBUG_LOG ("get malloc addr %p", res_addr);
	
	if(res_addr==NULL)
		free(malloc_work->addr_info);
	else
	{
		res_addr=dhmp_transfer_dhmp_addr(malloc_work->rdma_trans,
										malloc_work->addr_info->nvm_mr.addr);	// res_addr = 服务器节点编号（16bit）+ NVM虚拟地址（48bit）
		malloc_work->addr_info->node_index=dhmp_get_node_index_from_addr(res_addr);	 // 获得服务器节点id
		malloc_work->addr_info->write_flag=false;
		dhmp_addr_info_insert_ht(res_addr, malloc_work->addr_info);  	// 根据MurmurHash3_x86_32算法将res_addr插入到client->addr_info_ht中
	}
	
	malloc_work->res_addr=res_addr;
	malloc_work->done_flag=true;
}

// 获得nvm虚拟地址
void *dhmp_transfer_normal_addr(void *dhmp_addr)
{
	long long node_index, ll_addr;
	void *normal_addr;
	
	ll_addr=(long long)dhmp_addr;
	node_index=ll_addr>>48;

	node_index=node_index<<48;

	ll_addr-=node_index;
	normal_addr=(void*)ll_addr;

	return normal_addr;
}

/**
 *	dhmp_get_addr_info_from_ht:according to addr, find corresponding addr info
 */
struct dhmp_addr_info *dhmp_get_addr_info_from_ht(int index, void *dhmp_addr)
{
	struct dhmp_addr_info *addr_info;
	void *normal_addr;
	
	if(hlist_empty(&client->addr_info_ht[index]))
		goto out;
	else
	{
		normal_addr=dhmp_transfer_normal_addr(dhmp_addr);	
		hlist_for_each_entry(addr_info, &client->addr_info_ht[index], addr_entry)
		{
			if(addr_info->nvm_mr.addr==normal_addr)
				break;
		}
	}
	
	if(!addr_info)
		goto out;
	
	return addr_info;
out:
	return NULL;
}


void dhmp_free_work_handler(struct dhmp_work *work)
{
	struct dhmp_msg msg;
	struct dhmp_addr_info *addr_info;
	struct dhmp_free_request req_msg;
	struct dhmp_free_work *free_work;
	int index;

	free_work=(struct dhmp_free_work*)work->work_data;

	/*get nvm mr from client hash table*/
	index=dhmp_hash_in_client(free_work->dhmp_addr);
	addr_info=dhmp_get_addr_info_from_ht(index, free_work->dhmp_addr);
	if(!addr_info)
	{
		ERROR_LOG("free addr error.");
		goto out;
	}
	hlist_del(&addr_info->addr_entry);

	if(client->in_recovery == false){
		/*build a msg of free addr*/
		req_msg.addr_info=addr_info;
		memcpy(&req_msg.mr, &addr_info->nvm_mr, sizeof(struct ibv_mr));
		
		msg.msg_type=DHMP_MSG_FREE_REQUEST;
		msg.data_size=sizeof(struct dhmp_free_request);
		msg.data=&req_msg;
		
		DEBUG_LOG("free addr is %p length is %d",
			addr_info->nvm_mr.addr, addr_info->nvm_mr.length);

		dhmp_post_send(free_work->rdma_trans, &msg);

		/*wait for the server return result*/
		while(addr_info->nvm_mr.addr!=NULL);
	}
	
	free(addr_info);
out:
	free_work->done_flag=true;
}

void dhmp_read_work_handler(struct dhmp_work *work)
{
	struct dhmp_addr_info *addr_info;
	struct dhmp_rw_work *rwork;
	int index;
	struct timespec ts1,ts2;
	long long sleeptime;
	
	rwork=(struct dhmp_rw_work *)work->work_data;
	
	index=dhmp_hash_in_client(rwork->dhmp_addr);
	addr_info=dhmp_get_addr_info_from_ht(index, rwork->dhmp_addr);
	if(!addr_info)
	{
		ERROR_LOG("read addr is error.");
		goto out;
	}

	while(addr_info->write_flag);
	
	++addr_info->read_cnt;
#ifdef DHMP_CACHE_POLICY	
	if(addr_info->dram_mr.addr!=NULL)
	{
		dhmp_rdma_read(rwork->rdma_trans, &addr_info->dram_mr,
					rwork->local_addr, rwork->length, rwork->offset);	// WGT
	}
	else
	{
#endif	
		dhmp_rdma_read(rwork->rdma_trans, &addr_info->nvm_mr,
					rwork->local_addr, rwork->length, rwork->offset);	// WGT
#ifdef DHMP_CACHE_POLICY		
	}
#endif

out:
	rwork->done_flag=true;
}

// WGT
void dhmp_write_work_handler(struct dhmp_work *work)
{
	struct dhmp_addr_info *addr_info;
	struct dhmp_rw_work *wwork;
	int index;
	
	wwork=(struct dhmp_rw_work *)work->work_data;
	
	index=dhmp_hash_in_client(wwork->dhmp_addr);
	addr_info=dhmp_get_addr_info_from_ht(index, wwork->dhmp_addr);
	INFO_LOG("dhmp_write_work_handler index=%d, wwork->dhmp_addr=%p", index, wwork->dhmp_addr);
	if(!addr_info)
	{
		ERROR_LOG("write addr is error.");
		goto out;
	}

	/*check no the same addr write task in qp*/
	while(addr_info->write_flag);
	addr_info->write_flag=true;
	
	++addr_info->write_cnt;
#ifdef DHMP_CACHE_POLICY
	if(addr_info->dram_mr.addr!=NULL)
		dhmp_rdma_write(wwork->rdma_trans, addr_info, &addr_info->dram_mr,
						wwork->local_addr, wwork->length, wwork->offset);		// WGT
	else
	{
#endif
		dhmp_rdma_write(wwork->rdma_trans, addr_info, &addr_info->nvm_mr,
					wwork->local_addr, wwork->length, wwork->offset);			// WGT,为什么没有头文件声明这个函数？？？
#ifdef DHMP_CACHE_POLICY
	}
#endif
	INFO_LOG("dhmp_write_work_handler sucess!");
out:
	wwork->done_flag=true;
}

void dhmp_send_work_handler(struct dhmp_work *work)
{
	struct dhmp_addr_info *addr_info;
	struct dhmp_send_work *send_work;
	int index;
	
	send_work=(struct dhmp_send_work *)work->work_data;
	
	index=dhmp_hash_in_client(send_work->dhmp_addr);
	addr_info=dhmp_get_addr_info_from_ht(index, send_work->dhmp_addr);
	if(!addr_info)
	{
		ERROR_LOG("send addr is error.");
		goto out;
	}

	/*check no the same addr write task in qp*/
	while(addr_info->write_flag);
	addr_info->write_flag=true;

	if (send_work->is_write)
		++addr_info->write_cnt;
	else
		++addr_info->read_cnt;
#ifdef DHMP_CACHE_POLICY
	if(addr_info->dram_mr.addr!=NULL)
		dhmp_rdma_send(send_work, addr_info->dram_mr.addr);
	else
	{
#endif
		dhmp_rdma_send(send_work, addr_info->nvm_mr.addr);
#ifdef DHMP_CACHE_POLICY
	}
#endif

out:
	addr_info->write_flag=false;
	send_work->done_flag=true;
}

dhmp_close_work_handler(struct dhmp_work *work)
{
	struct dhmp_close_work *cwork;
	struct dhmp_msg msg;
	int tmp=0;
	
	cwork=(struct dhmp_close_work*)work->work_data;

	msg.msg_type=DHMP_MSG_CLOSE_CONNECTION;
	msg.data_size=sizeof(int);
	msg.data=&tmp;
	
	dhmp_post_send(cwork->rdma_trans, &msg);

	cwork->done_flag=true;
}

void *dhmp_work_handle_thread(void *data)
{
	struct dhmp_work *work;

	while(client && !client->ctx.stop)
	{
		work=NULL;
		
		pthread_mutex_lock(&client->mutex_work_list);
		if(!list_empty(&client->work_list))
		{
			work=list_first_entry(&client->work_list, struct dhmp_work, work_entry);
			list_del(&work->work_entry);
		}
		pthread_mutex_unlock(&client->mutex_work_list);

		if(work)
		{
			switch (work->work_type)
			{
				case DHMP_WORK_MALLOC:
					dhmp_malloc_work_handler(work);
					break;
				case DHMP_WORK_FREE:
					dhmp_free_work_handler(work);
					break;
				case DHMP_WORK_READ:
					dhmp_read_work_handler(work);
					break;
				case DHMP_WORK_WRITE:
					dhmp_write_work_handler(work);
					break;
				case DHMP_WORK_SEND:
					dhmp_send_work_handler(work);
					break;
				case DHMP_WORK_POLL:
					dhmp_poll_ht_func();
					break;
				case DHMP_WORK_CLOSE:
					dhmp_close_work_handler(work);
					break;
				default:
					ERROR_LOG("work exist error.");
					break;
			}
		}
		
	}

	return NULL;
}

