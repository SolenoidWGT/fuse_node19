//#include "../dhmp_client/include/dhmp_hash.h"
#include "../../include/dhmp_fs.h"
#include "../dhmp_client/include/linux/list.h"

int murmur3_32(const char *key, int len, int seed);
typedef struct hash_node
{
    /* data */
    int                 server_id;
    char	            ip_addr[DHMP_ADDR_LEN];
    uint32_t            key;
    struct list_head    list;
}hash_node;


hash_node * hash_node_constructor(int server_id, char * ip)
{
    hash_node * node = (hash_node *)malloc(sizeof(hash_node));
    node->server_id = server_id;
    node->key = -1;
    int i;
    if(ip != NULL)
        memcpy(node->ip_addr, ip, DHMP_ADDR_LEN);
    printf("sucess constructor %d\n", server_id);
    return node;
}

void hash_node_deconstructor(hash_node * node)
{
    free(node);
}


struct fuse_consistent_hash
{
    /* data */


};


hash_node * init_consistent_hash(int serverNum, server_node ** server_list)
{
    int i;
    hash_node * hash_head = hash_node_constructor(-1, NULL); // 头节点不适应
    INIT_LIST_HEAD(&hash_head->list);
    hash_node * head = hash_head;
    for(i=0; i<serverNum; i++){
        hash_node * tp = hash_node_constructor(i, server_list[i]->ip_addr);
        tp->key = murmur3_32(tp->ip_addr, DHMP_ADDR_LEN,17);
        printf("tp->key is %ld\n",  tp->key);
        hash_add_server(head, tp);
        // while(!hash_add_server(head, tp)){
        //     tp->key = murmur3_32(tp->ip_addr, DHMP_ADDR_LEN,17);
        // }
        printf("sucess add node %d\n", i);
    }
    return hash_head;
}


int hash_add_server(hash_node * head, hash_node * new_node)
{
    hash_node * iter_node = NULL, * pre_node = NULL;
    if(list_empty(&head->list)){
        printf("only head!\n");
        list_add_tail(&new_node->list, &head->list);
        return 1;
    }
    list_for_each_entry(iter_node, &head->list, list)
    {
        if(iter_node->key < new_node->key){
            printf("move!\n");
            pre_node = iter_node;
        }else if(iter_node->key > new_node->key){
            printf("add!\n");
            list_add(&new_node->list, &pre_node->list);
            return 1;
        }else{
            printf("error!\n");
            // hash值冲突？重新hash？
            return -1;
        }
    }
    printf("last!\n");
    list_add_tail(&new_node->list, &head->list);
    return 1;
}

void show_list()
{


}


void hash_del_server(hash_node * head, hash_node * new_node)
{


}

char addr1[DHMP_ADDR_LEN] = "10.10.10.1";
char addr2[DHMP_ADDR_LEN] = "10.10.10.2";
char addr3[DHMP_ADDR_LEN] = "10.10.10.3";

int main(){
    int i, j;
    int SERVERNUM = 3;
    char ** test_ip = (char **)malloc(SERVERNUM * sizeof(char *));
    for(i=0; i<SERVERNUM;i++){
        char * ip = malloc(DHMP_ADDR_LEN);
        test_ip[i] = ip;
        memset(test_ip[i], 1, DHMP_ADDR_LEN);
        test_ip[i][DHMP_ADDR_LEN-1] = i;
    
        for(j=0; j<DHMP_ADDR_LEN;j++ ){
            printf("%c", test_ip[i][j] + '0');
        }
        printf("\n");
    }

    server_node ** fuse_sList = (server_node **) malloc(SERVERNUM * sizeof(uintptr_t));
    int server_i, init_bank_i;
    for(server_i=0; server_i< SERVERNUM; server_i++){
        // 分配文件系统的空间
        fuse_sList[server_i] = (server_node*)malloc(sizeof(server_node));
        fuse_sList[server_i]->server_id = server_i;
        fuse_sList[server_i]->alive = 1;
        memcpy(fuse_sList[server_i]->ip_addr, test_ip[server_i], DHMP_ADDR_LEN);
        fuse_sList[server_i]->test_alive_addr = NULL;	 // 给判活数组分配空间
        for(init_bank_i = 0; init_bank_i < BANK_NUM; init_bank_i++){
            fuse_sList[server_i]->bank[init_bank_i] = NULL;
        }
        printf("sucess init node %d\n", server_i);
    }


    hash_node * head = init_consistent_hash(SERVERNUM, fuse_sList);
    hash_node * iter_node = NULL;
    list_for_each_entry(iter_node, &head->list, list)
    {
         printf("server_id is %d, server key is %ld\n", iter_node->server_id, iter_node->key);

    }
    for(i=0; i<SERVERNUM; i++){
        free(test_ip[i]);
    }
    free(test_ip);

    return 0;
}

int murmur3_32(const char *key, int len, int seed)
{
	static const int c1 = 0xcc9e2d51;
	static const int c2 = 0x1b873593;
	static const int r1 = 15;
	static const int r2 = 13;
	static const int m = 5;
	static const int n = 0xe6546b64;
 
	int hash = seed;
 
	const int nblocks = len / 4;
	const int *blocks = (const int *)key;
	int i;
	for (i = 0; i < nblocks; i++)
	{
		int k = blocks[i];
		k *= c1;
		k = (k << r1) | (k >> (32 - r1));
		k *= c2;
 
 
		hash ^= k;
		hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
	}
 
	const int *tail = (const int *)(key + nblocks * 4);
	int k1 = 0;
 
	switch (len & 3)
	{
	case 3:
		k1 ^= tail[2] << 16;
	case 2:
		k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];
 
 
		k1 *= c1;
		k1 = (k1 << r1) | (k1 >> (32 - r1));
		k1 *= c2;
		hash ^= k1;
	}
 
	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);
 
	return hash;
}