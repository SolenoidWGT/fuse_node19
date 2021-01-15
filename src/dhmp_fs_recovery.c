#include "../include/dhmp_fs.h"
#include "../include/dhmp_fs_logs.h"


#ifdef DHMP_ON
void judege_alive(){
	int i;
	for(i=0; i<SERVERNUM; i++){
		if(fuse_sList[i]->alive){
			if(dhmp_send(fuse_sList[i]->test_alive_addr, alive_context, CHUNK_SIZE, true) == -1){
				// 服务器断线
				fuse_sList[i]->alive = 0;
			}
		}else{
			// 尝试重连
		}
	}
}

server_node * find_alive_server(){
	int i;
	for(i=0; i<SERVERNUM; i++){
		if(fuse_sList[i]->server_state == CONNECTED)
			return fuse_sList[i];
	}
	return NULL;
}


void fuse_recover(int re_server_id)
{
	if(recovery_in_use){
			FUSE_ERROR_LOG("RECOVERY: fuse_recover already start!");
		return;
	}
	recovery_in_use= true;

	//FUSE_INFO_LOG("RECOVERY: FUSE start to recover server %d!", re_server_id);

	server_node * recover_server = fuse_sList[re_server_id];
	recover_server->server_state = RECONNECT;

	// if(recover_server->server_state == RECONNECT){
	// 	FUSE_ERROR_LOG("Recover non RECONNECT server, return！");
	// 	return -1;
	// }

	int i, alive_num=0, re;
	bool single_node_recovery = false;
	rw_task * recover_task = NULL;
	bool bank_flag[BANK_NUM];
	memset(bank_flag, false, sizeof(bool)*BANK_NUM);
	unsigned long long total_write_time = 0;
	struct timespec start_test, end_test;

	for(i=0; i<SERVERNUM; i++){
		if(fuse_sList[i]->server_state == CONNECTED){
			alive_num++;
		}
	}

	if(alive_num == 1){
		single_node_recovery = true;
	}else if(alive_num == 0){
		FUSE_ERROR_LOG("RECOVERY: Alive Servers number less than 2, fuse_recover FAIL！");
		return;
	}

	bool result = false;
	//FUSE_INFO_LOG("RECOVERY: Start to reMalloc!");

	for(i = 0; i < BANK_NUM; i++){
		dhmp_free(recover_server->bank[i]);
	}

	// 重新为被恢复的server的bank分配内存
	for(i = 0; i < BANK_NUM; i++){
		recover_server->bank[i] = dhmp_malloc(BANK_SIZE, recover_server->server_id);
	}
	//FUSE_INFO_LOG("RECOVERY: reMalloc is over!");

	server_node * active_recover_server = find_alive_server();

	if(!single_node_recovery){
		pthread_mutex_lock(&active_recover_server->server_mutex);	// 暂停恢复服务器的写入服务
	}else{
		pthread_mutex_lock(&fuse_mutex);	// 暂停所有服务
		//FUSE_INFO_LOG("RECOVERY: Single_node_recovery stop all write server!");
		clock_gettime(CLOCK_MONOTONIC, &start_test);
	}
	//FUSE_INFO_LOG("RECOVERY: Stop active_recover %d 's write server!", active_recover_server->server_id);

	active_recover_server->server_state = RECOVERING;

	void * recover_bank_context = malloc(BANK_SIZE);

	if(use_bank_len == BANK_NUM){
		for(i=0; i<BANK_NUM; i++){
			re = dhmp_read(active_recover_server->bank[i], recover_bank_context, BANK_SIZE, 0);
			if(re == -1){
				// 恢复服务器掉线！
				active_recover_server->server_state = DISCONNECT;
				if(!single_node_recovery)
					goto cleanRecover1;
				else
					goto cleanRecover;
			}
			re = dhmp_write(recover_server->bank[i], recover_bank_context, BANK_SIZE, 0);
			if(re == -1){
				// 被恢复服务器掉线！
				recover_server->server_state = DISCONNECT;
				if(!single_node_recovery)
					goto cleanRecover1;
				else
					goto cleanRecover;
			}
		}
	}else{
		recover_task = NULL;
		int rewrite_count = 0 ;
		list_for_each_entry(recover_task, &fuse_journal_list->list, list){

			int bank_id = recover_task->bank_id;			// 恢复的bank_id
			if(bank_flag[bank_id]){
				continue;
			}
			dhmp_read(active_recover_server->bank[bank_id], recover_bank_context, BANK_SIZE, 0);

			dhmp_write(recover_server->bank[bank_id], recover_bank_context, BANK_SIZE, 0);
			rewrite_count++;
			bank_flag[bank_id] = 1;
		}
		//FUSE_INFO_LOG("Single_node_recovery has rewarite %d banks", rewrite_count);
	}

	//FUSE_INFO_LOG("RECOVERY: Copy is over!");
	if(!single_node_recovery){
		pthread_mutex_lock(&fuse_mutex);  // 获取文件系统全局修改锁：等待所有的write操作结束，之后暂停所有服务器的write操作
		//FUSE_INFO_LOG("RECOVERY: Stop all write server!");
	}else{
		clock_gettime(CLOCK_MONOTONIC, &end_test); 
		total_write_time = ((end_test.tv_sec * 1000000000) + end_test.tv_nsec) -
				((start_test.tv_sec * 1000000000) + start_test.tv_nsec); 
		double total_time = (double)(total_write_time/1000000);
		//FUSE_INFO_LOG("RECOVERY: Single_node_recovery Sucess!, recovery time is %lf ms", total_time);
		result=true;
		active_recover_server->server_state = CONNECTED;
		recover_server->server_state = CONNECTED;
		goto cleanRecover;
	}
	
	recover_task = NULL;
	rw_task * recover_task_next= NULL;
	// 遍历重写缓冲区的内容
    list_for_each_entry(recover_task, &active_recover_server->rw_task_head->list, list)
    {
		size_t size = recover_task->size;
		off_t offset = recover_task->bank_offset;           // 写入位置
		int bank_id = recover_task->bank_id;			// 恢复的bank_id
		// void * tbank = (fuse_sList[i]->bank[bank_id]);  

		// 找到一个不参与备份的第三方服务器，获取最新的内容
		server_node * latest_server = find_alive_server();
		//FUSE_INFO_LOG("RECOVERY: Third party server is %d", latest_server->server_id);
		//FUSE_INFO_LOG("RECOVERY: Recovery bank_id is %d", bank_id);
		
		// 
		re = dhmp_read(latest_server->bank[bank_id], recover_bank_context, BANK_SIZE, 0);
		
		if(re == -1){
			// 第三方服务器掉线！
			FUSE_ERROR_LOG("RECOVERY: Third party server %d is down", latest_server->server_id);
			latest_server->server_state = DISCONNECT;
			goto cleanRecover;
		}

		//  dhmp_write(tbank, buf, size, offset);
		re = dhmp_write(active_recover_server->bank[bank_id], recover_bank_context, size, offset);
		if(re == -1){
			// 主动恢复服务器掉线！
			FUSE_ERROR_LOG("RECOVERY: active_recover_server %d is down", active_recover_server->server_id);
			active_recover_server->server_state = DISCONNECT;
			goto cleanRecover;
		}
		re = dhmp_write(recover_server->bank[bank_id], recover_bank_context, size, offset);
		if(re == -1){
			// 被恢复服务器掉线！
			FUSE_ERROR_LOG("RECOVERY: recover_server %d is down", recover_server->server_id);
			recover_server->server_state = DISCONNECT;
			goto cleanRecover;
		}
    }
	//FUSE_INFO_LOG("RECOVERY: Write all reWrite buffer context!");
	list_for_each_entry_safe(recover_task, recover_task_next, &active_recover_server->rw_task_head->list, list)
	{
		free(recover_task);
		active_recover_server->rw_tasks_len--;
	}

	//FUSE_INFO_LOG("RECOVERY: Free reWrite buffer context!");

	active_recover_server->server_state = CONNECTED;
	recover_server->server_state = CONNECTED;
	result=true;

cleanRecover:
	pthread_mutex_unlock(&fuse_mutex); 
cleanRecover1:
	if(!single_node_recovery)
		pthread_mutex_unlock(&active_recover_server->server_mutex);
	free(recover_bank_context);
	recovery_in_use=false;
	if(result)
		goto sucess;
	else
		goto fail;

sucess:
	FUSE_INFO_LOG("RECOVERY: Recovery sucess!");
	//return 1;

fail:
	FUSE_INFO_LOG("RECOVERY: Recovery Fail!");
	//return -1;
}

#endif