#include <stdlib.h>
#include <stdio.h>
#include "../include/list.h"
typedef struct node
{
    int id;
    int is_dir;
    struct list_head d_bro;
	struct list_head d_son;
}Node;

int main()
{
    Node nlist[20];
    int i =0 ;
    for(i =0; i <20; i++){
        Node tp_node;
        tp_node.is_dir = 0;
        tp_node.id = i;
        nlist[i] = tp_node;
    }
    // nlist[0].d_son

    INIT_LIST_HEAD(&nlist[0].d_son);	
    list_add_tail(&nlist[1].d_bro, &nlist[0].d_son);		// 将该文件的inode加入到他的父亲节点的儿子列表中
    list_add_tail(&nlist[2].d_bro, &nlist[0].d_son);
    list_add_tail(&nlist[3].d_bro, &nlist[0].d_son);
    list_add_tail(&nlist[4].d_bro, &nlist[0].d_son);
    list_add_tail(&nlist[5].d_bro, &nlist[0].d_son);

    Node *head_son = NULL, * head = &nlist[0];
    printf("Now head id is %d\n", head->id);
    list_for_each_entry(head_son, &head->d_son, d_bro)
    {
        printf("node id is %d\n", head_son->id);
    }
    INIT_LIST_HEAD(&nlist[1].d_son);
    list_add_tail(&nlist[6].d_bro, &nlist[1].d_son);
    list_add_tail(&nlist[7].d_bro, &nlist[1].d_son);
    list_add_tail(&nlist[8].d_bro, &nlist[1].d_son);

    head = &nlist[1];
    head_son= NULL;
    printf("Now head id is %d\n", head->id);
    list_for_each_entry(head_son, &head->d_son, d_bro)
    {
        printf("node id is %d\n", head_son->id);
    }
    
    list_del(&nlist[6].d_bro);
    list_del(&nlist[7].d_bro);
    list_del(&nlist[8].d_bro);

    head = &nlist[1];
    head_son= NULL;
    printf("Now head id is %d\n", head->id);
    list_for_each_entry(head_son, &head->d_son, d_bro)
    {
        printf("node id is %d\n", head_son->id);
    }
    list_add_tail(&nlist[6].d_bro, &nlist[1].d_son);
    list_add_tail(&nlist[7].d_bro, &nlist[1].d_son);
    list_add_tail(&nlist[8].d_bro, &nlist[1].d_son);

    head = &nlist[1];
    head_son= NULL;
    printf("Now head id is %d\n", head->id);
    list_for_each_entry(head_son, &head->d_son, d_bro)
    {
        printf("node id is %d\n", head_son->id);
    }

    head = &nlist[1];
    head_son= NULL;
    printf("Now head id is %d\n", head->id);
    list_for_each_entry(head_son, &head->d_son, d_bro)
    {
        printf("node id is %d\n", head_son->id);
    }

    head = &nlist[1];
    head_son= NULL;
    printf("Now head id is %d\n", head->id);
    list_for_each_entry(head_son, &head->d_son, d_bro)
    {
        printf("node id is %d\n", head_son->id);
    }


}