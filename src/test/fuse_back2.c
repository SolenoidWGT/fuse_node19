#define FUSE_USE_VERSION 31

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#ifdef linux
#define _XOPEN_SOURCE 700
#endif

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


#define DEBUG_ON
#define DHMP_ON

#ifdef DHMP_ON
#include "./dhmp_client/include/dhmp.h"
#endif

//#define TOTOL_SIZE ((uint64_t)1024*1024*1024*2)     // 磁盘总空间,2G
#define TOTOL_SIZE ((uint64_t)1024*1024*20)     // 磁盘总空间,2M
//#define BANK_SIZE (1024*1024*4)                     // BANK大小，4M
#define BANK_SIZE ((uint64_t)1024*20)          // BANK大小，2M
#define BANK_NUM (TOTOL_SIZE/BANK_SIZE)             // dhmp_malloc的数量
#define CHUNK_SIZE (1024*4)                        // 单个数据块的大小，64K
#define CHUNK_NUM (TOTOL_SIZE/CHUNK_SIZE)           // 数据块的总数

#define FILE_NAME_LEN 1024

#define ACCESS_SIZE 65536

struct context{
	int chunk_index;
	size_t size;
	struct context * next;
};

struct inode{ 
	char filename[FILE_NAME_LEN];
	size_t size;
	time_t timeLastModified;
	char isDirectories;
	
		struct{
			struct context * context;
		};
		struct {
			struct inode * son;
			struct inode * bro;
		};
	
};

struct attr {
	int size;
	time_t timeLastModified;
	char isDirectories;
};

struct inode_list{
	struct inode_list * next;
	char isDirectories;
	char filename[FILE_NAME_LEN];
};

void * bank[BANK_NUM];      // bank[init_bank_i] = dhmp_malloc(BANK_SIZE,0);
char * dhmp_local_buf;      // 
struct inode *root;         // 根节点

FILE * fp;
char msg[1024];
char msg_tmp[1024];
#ifdef DEBUG_ON
	#define DEBUG(x) {sprintf(msg," %s ",x); fwrite(msg, strlen(msg), 1, fp);	fflush(fp);}
	#define DEBUG_INT(x) {sprintf(msg_tmp," %d\t",(int)x);DEBUG(msg_tmp);}
	#define DEBUG_POINTER(x) {sprintf(msg_tmp," %p\t", (void*)x);DEBUG(msg_tmp);}
	#define DEBUG_END() {sprintf(msg,"\n"); fwrite(msg, strlen(msg), 1, fp);	fflush(fp);}
	#define DEBUG_P(x)  {sprintf(msg_tmp," %x\t",x);DEBUG(msg_tmp);}
#else
	#define DEBUG(x) {}
	#define DEBUG_INT(x) {}
	#define DEBUG_END() {}
	#define DEBUG_P(x) {}
#endif
char bitmap[CHUNK_NUM];



// 全局变量，非线程安全
int SERVERNUM=0;
#define SSERVERNUM (SERVERNUM+1)
typedef struct server_list{
    struct list_head list;
    int server_id;
	void * bank[BANK_NUM]; 
}server_list;
int read_server=0;

server_list **fuse_sList;
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
#ifdef DHMP_ON
		dhmp_client_init(BANK_SIZE);
		DEBUG("dhmp_client_init SUCESS!");
		DEBUG_END();
		SERVERNUM = dhmp_get_trans_count();
		if(SERVERNUM==0){
			DEBUG("dhmp_get_trans_count ERROR ! SERVERNUM==0");
			DEBUG_END();
		}
		else{
			DEBUG("dhmp_get_trans_count SUCCESS ! SERVERNUM =");
			DEBUG_INT(SERVERNUM);
			DEBUG_END();
		}
		// fuse_sList = (server_list **) malloc(SSERVERNUM * sizeof(uintptr_t));
		// for(server_i=0; server_i< SSERVERNUM; server_i++){
		// 	fuse_sList[server_i] = (server_list*)malloc(sizeof(server_list));
		// 	if(server_i==0){
		// 		INIT_LIST_HEAD(&fuse_sList[server_i]->list);       // 头节点不使用
		// 	}else{
		// 		fuse_sList[server_i]->server_id = server_i;
		// 		for(init_bank_i = 0;init_bank_i < BANK_NUM;init_bank_i++){
		// 			fuse_sList[server_i]->bank[init_bank_i] = dhmp_malloc(BANK_SIZE, server_i);
		// 		}
		// 		list_add_tail(&fuse_sList[server_i]->list, fuse_sList[0]);
		// 	}
		// }
		fuse_sList = (server_list **) malloc(SERVERNUM * sizeof(uintptr_t));
		for(server_i=0; server_i< SERVERNUM; server_i++){
			fuse_sList[server_i] = (server_list*)malloc(sizeof(server_list));
			fuse_sList[server_i]->server_id = server_i;
			for(init_bank_i = 0; init_bank_i < BANK_NUM; init_bank_i++){
				fuse_sList[server_i]->bank[init_bank_i] = dhmp_malloc(BANK_SIZE, server_i);
			}
			DEBUG("dhmp_server");
			DEBUG_INT(server_i);
			DEBUG("malloc memory is SUCCESS !");
			DEBUG_END();
		}
#else
                for(init_bank_i = 0;init_bank_i < BANK_NUM;init_bank_i++)
                        bank[init_bank_i] = malloc(BANK_SIZE);
#endif	
	root = (struct inode*) malloc(sizeof(struct inode));
	root -> bro = NULL;
	root -> son = NULL;
	root -> isDirectories = 1;              // 是目录
	root -> size = 0;
	root -> timeLastModified = time(NULL);  
	memset(root->filename,0,FILE_NAME_LEN);
	root->filename[0] = '/';                // 文件名为“/”
	memset(bitmap,0,sizeof(bitmap));        // 初始化所有的chunk为可用状态

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


int getFreeChunk()
{
	int i =0;
	for(;i < CHUNK_NUM; i++)
	{
		if(bitmap[i] == 1)continue;
		break;
	}
	if(i == CHUNK_NUM)
		DEBUG("Error:no space for free chunk");
	bitmap[i] = 1;
	return i;
}

void dhmp_fs_read_from_bank( int chunk_index, char * buf, size_t size, off_t chunk_offset)
{
DEBUG("dhmp_fs_read_from_bank");
        DEBUG_INT(chunk_index);   
        DEBUG_INT(size);
        DEBUG_INT(chunk_offset);
        DEBUG(" |-| ");
		int times = chunk_index % (CHUNK_NUM/BANK_NUM);
		off_t offset = chunk_offset + times * CHUNK_SIZE;
#ifndef DHMP_ON
	void * tbank = (bank[chunk_index / (CHUNK_NUM/BANK_NUM)]);
	memcpy(buf, tbank + offset, size);
	DEBUG("sucess:");
	DEBUG_INT(0); 
	DEBUG_END();
	return;
#else
	//int dhmp_read(void *dhmp_addr, void * local_buf, size_t length);
	int * server_state = (int*) malloc(sizeof(int)*SERVERNUM);
	int i;
	//for(i=0; i< 1; i++){	// 默认只从server0读取数据
	while(1){
		DEBUG_END();
		DEBUG("dhmp_read:");
		DEBUG("serverid:");
		DEBUG_INT(fuse_sList[read_server]->server_id);
		if(server_state[i] == -1){
			DEBUG("server Down! Change read server");
			DEBUG_END();
			read_server = (read_server+1)%SERVERNUM;
			if(dhmp_get_trans_count() == 0){
				DEBUG("server ");
			}
			continue;
		}

		void * tbank = (fuse_sList[read_server]->bank[chunk_index / (CHUNK_NUM/BANK_NUM)]);
		DEBUG("tbank:");
		DEBUG_POINTER(tbank); 
		DEBUG(", offset:");
		DEBUG_INT(offset);
		DEBUG(", tbank + offset:");
		DEBUG_POINTER((tbank + offset)); 
		DEBUG(", size:");
		DEBUG_INT(size); 
		DEBUG_END();
		int re = dhmp_read(tbank, buf, size, offset);
		break;
	}
	read_server = (read_server+1)%SERVERNUM;
	//}
	free(server_state);
	return;
#endif	
}

void dhmp_fs_write_to_bank(int chunk_index, char * buf, size_t size, off_t chunk_offset)
{

DEBUG("dhmp_fs_write_to_bank");
        DEBUG_INT(chunk_index);  
        DEBUG_INT(size);
        DEBUG_INT(chunk_offset);
		DEBUG(" |-| ");
	int times = chunk_index % (CHUNK_NUM/BANK_NUM);             // 需要跳过多少个chunk
	off_t offset = chunk_offset + times * CHUNK_SIZE;           // 写入位置
#ifndef DHMP_ON
		// CHUNK_NUM/BANK_NUM 平均一个bank里面有多少个chunk
		void * tbank = (bank[chunk_index / (CHUNK_NUM/BANK_NUM)]);  // 获得bank的index（需要跳过多少个bank）
		memcpy(tbank + offset, buf, size);
		DEBUG("sucess:");
		DEBUG_INT(0); 
		DEBUG_END();
		return ;
#else
	int i;
	DEBUG_END();
	int * server_state = (int*) malloc(sizeof(int)*SERVERNUM);
	dhmp_test_trans_state(server_state, SERVERNUM);
	for(i=0; i<SERVERNUM; i++){
		DEBUG("dhmp_write ");
		DEBUG("serverid:");
		DEBUG_INT(fuse_sList[i]->server_id);
		if(server_state[i] == -1){
			DEBUG("server Down! Stop write");
			DEBUG_END();
			continue;
		}
		void * tbank = (fuse_sList[i]->bank[chunk_index / (CHUNK_NUM/BANK_NUM)]);  
		DEBUG("tbank:");
		DEBUG_POINTER(tbank); 
		DEBUG(", offset:");
		DEBUG_INT(offset);
		DEBUG(", tbank + offset:");
		DEBUG_POINTER((tbank + offset)); 
		DEBUG(", size:");
		DEBUG_INT(size); 
		DEBUG_END();
		//int dhmp_write(void *dhmp_addr, void * local_buf, size_t length);
		int re = dhmp_write(tbank, buf, size, offset);
	}
	free(server_state);
	return;
#endif
}



/**
 * Check file access permissions
 *
 * This will be called for the access() and chdir() system
 * calls.  If the 'default_permissions' mount option is given,
 * this method is not called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * If this request is answered with an error code of ENOSYS, this is
 * treated as a permanent success, i.e. this and all future access()
 * requests will succeed without being send to the filesystem process.
 *
 * Valid replies:
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param mask requested access mode
 */
static int dhmp_fs_access(const char *path, int mask)
{
	return 0;
}



/* deal path to dir/file mode
 * 	/root/lhd/node  to /root/lhd/ node
 * 		/root/lhd/node/ to /root/lhd/ node
 * 			do not deal root dir : /
 * 			*/
void deal(const char *path,char * dirname,char * filename)
{
	int i =0,j=0,k = 0,m=0;
	int len = strlen(path);
	for(;i < len;i++)
	{
		dirname[i] = path[i];
		if(path[i] == '/' && path[i+1] != 0) j = i;
	}
	m = j++;
	for(;j < len;)
	{
		filename[k] = path[j];
		k++;j++;
	}
	dirname[m+1] = 0;
	if(filename[k-1] == '/') k--;
	filename[k] = 0;
/*	printf("deal %s %s %s\n",path,dirname,filename);*/
}


// dirname形如：“/home/gtwang/test.c/"
// 疑问：这个函数是根据dirname获取对应的inode节点
// 但为什么函数名是get_father_inode，而不是get_inode
struct inode * get_father_inode(char *dirname)
{
	struct inode* head = root;
	char s[FILE_NAME_LEN];
	int i = 1,j,is_find = 0;
	if(strlen(dirname) == 1)
	{
        // 文件名长度为1则一定是根节点
		return head;
	}
	int len = strlen(dirname);
    // 需要在文件路径的最后补上一个'/'
	if(dirname[len-1] != '/'){
		dirname[len] = '/';  
		dirname[len+1] = 0;  
	} 
	while(1)
	{
        // 从根节点开始遍历
		head = head->son;
		if(head == NULL) break;	
		j = 0;	
		while(1){
			s[j++] = dirname[i++];
			if(dirname[i] == '/') // 获得了一个完整的文件名
			{
				s[j] = 0;   // '\0'的ascill码为0
				while(1){
                    // 在同级的目录（bro链表）中遍历查找是否含有该文件名的文件
                    // 注意文件不能重名
					if(strcmp(head->filename,s) == 0) 
					{
						is_find = 1;
						break;//to 1
					}
					head = head -> bro;
					if(head == NULL) return NULL;
				}
			}
			if(is_find == 1) {
				is_find = 0;
				if(!dirname[i+1])   // 如果已经来到了'\0'，则所有的路径分量已经搜索完毕
					is_find = 1;
				break;
			}
		}
        // 跳过当前的'/',继续遍历下一个路径分量
		if(!dirname[++i]) break;
	}
	if(is_find == 1)
	{
		return head;
	}
	return NULL;
}


int dhmpMknod(const char * path)
{
	struct attr attr;
	if(strlen(path) == 1) return 0;
	char filename[FILE_NAME_LEN],dirname[FILE_NAME_LEN];
	deal(path,dirname,filename);    // 将文件所在目录和文件名进行分割
	struct inode * father = get_father_inode(dirname);  // 获得目录的inode
	if(father == NULL) {DEBUG("EROrr path to mknod\n");return -1;}
	{
		char filename[FILE_NAME_LEN];
		int len = strlen(path);
		strcpy(filename,path);
		filename[len] = '/'; filename[len+1] = 0;
		struct inode* head = get_father_inode(filename);
		if(head != NULL)    // 检查是否有重名的文件，如果有返回-1
		{
			return  -1;
		} 
	}
	struct inode * now = malloc(sizeof(struct inode));;
	now -> isDirectories = 0;
	now -> son = NULL;
	now -> bro = NULL;
	now -> size = 0;
	now -> timeLastModified = time(NULL);
	now -> context = NULL;
	memset(now->filename,0,FILE_NAME_LEN);
	strcpy(now->filename,filename);
	struct inode * tmp = father->son;
	father -> son = now;
	now -> bro = tmp;
	return 1;
	
}

int dhmpCreateDirectory(const char * path)
{

	if(strlen(path) == 1) return 0;
	char filename[FILE_NAME_LEN],dirname[FILE_NAME_LEN];
	deal(path,dirname,filename);

	struct inode * now = malloc(sizeof(struct inode));;
	now -> isDirectories = 1;  //////////////////////////////////
	now -> son = NULL;
	now -> size = 0;
	now -> timeLastModified = time(NULL);
	memset(now->filename,0,FILE_NAME_LEN);
	strcpy(now->filename,filename);
	struct inode * father = get_father_inode(dirname);
    // 疑问：为什么创建文件夹的时候不会检查是否有重名文件夹？
	if(father == NULL) return -1;
	if(father->isDirectories == 0) return -1;
	struct inode * tmp = father->son;
	father -> son = now;
	now -> bro = tmp;
	return 1;
}


// 根据路径名获得文件属性
int dhmpGetAttr(const char *path,struct attr *attr)
{
	char filename[FILE_NAME_LEN];
	int len = strlen(path);
	strcpy(filename,path);
	filename[len] = '/'; 
    filename[len+1] = 0;
	struct inode* head = get_father_inode(filename);        // 根据文件名获得对应的inode
	if(head == NULL) return -1;
	attr->isDirectories = head->isDirectories;
	attr->size = head->size;
	attr->timeLastModified = head->timeLastModified;
	DEBUG("get atter size =");
	DEBUG_INT( attr->size );
	DEBUG_END();
	return 0;
}

// 作用：将一个文件夹下的所有文件返回
int dhmpReadDir(const char *path,struct inode_list *Li)
{
	char filename[FILE_NAME_LEN];
	struct inode_list *list=NULL;
	int len = strlen(path);
	Li->isDirectories = -1;
	Li->next = NULL;
	struct inode* head;
	if(len != 1)
	{
		strcpy(filename,path);
		head = get_father_inode(filename);
	}
	else head = root;
	if(head == NULL) return -1;

    // 如果head是一个文件的化那么son会是null，因此返回的Li->isDirectories也会是-1
    // 如果head是一个目录，那么其会进入该目录，并将该目录下所有的文件都链接到Li->next上
    // 注意：如果head是一个空目录的话，head->son也会是NULL，空文件夹的话和文件是没有区别的
	head = head->son;   
	if(head != NULL)
	{
		Li->isDirectories = head->isDirectories;
		strcpy(Li->filename,head->filename);
		Li->next = NULL;
		head = head->bro;
	}
	list = Li;
	while(head != NULL)
	{
		list->next = malloc(sizeof(struct inode_list));
		list = list->next;
		list->isDirectories = head->isDirectories;
		strcpy(list->filename,head->filename);
		list->next = NULL;
		head = head->bro;
	}
	return 0;
}

void dhmpFreeInode(struct inode * head)
{
	if(head == NULL) return ;
	struct context * context = head->context, *tmp;
    // 把该文件下所有的chunk都释放掉
	while(context != NULL)
	{
		tmp = context;
		bitmap[tmp->chunk_index] = 0;   // 归还分配的内存
		context = context->next;
		free(tmp);
	}
	free(head); // 释放inode
}

// 递归遍历，删除该文件夹下所有文件
void dhmpDeleteALL(struct inode *head)
{
	if(head == NULL) return;
	if(head->bro != NULL)
	{
		dhmpDeleteALL(head->bro);
	}
	if(head->isDirectories == 1 && head->son != NULL)
	{
		dhmpDeleteALL(head->son);
	}
	dhmpFreeInode(head);
}

int dhmpDelFromInode(struct inode *head,char * filename)
{
	struct inode * tmp ,*ttmp;
	tmp = head -> son;
	if(tmp == NULL) return 0;
    // 在其父目录下搜索该文件进行删除
	if(strcmp(filename,tmp->filename) == 0)
	{
		head -> son = tmp ->bro;
		tmp -> bro = NULL;
		if(tmp->isDirectories == 1) // 如果是文件夹，还需要删除其下所有的文件
			dhmpDeleteALL(tmp->son);
		dhmpFreeInode(tmp);
		return 0;
	}
	while(tmp->bro!=NULL)
	{
		ttmp = tmp;
		tmp = tmp->bro;
		if(strcmp(filename,tmp->filename) == 0)
		{
			ttmp -> bro = tmp ->bro;
			tmp->bro = NULL;
			if(tmp->isDirectories == 1)
				dhmpDeleteALL(tmp->son);
			dhmpFreeInode(tmp);
			return 0;
		}
	}
    // 没有找到filename，删除失败
	return -1;
}

// 删除目录和文件没什么区别
int dhmpDelete(const char *path)
{
	if(strlen(path) == 1) return 0;
	char filename[FILE_NAME_LEN],dirname[FILE_NAME_LEN];
	deal(path,dirname,filename);
	struct inode * father = get_father_inode(dirname);
	return dhmpDelFromInode(father,filename);
}

/** 
 * 
 * Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given. In that case it is passed to userspace,
 * but libfuse and the kernel will still assign a different
 * inode for internal use (called the "nodeid").
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 */
static int dhmp_fs_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
	struct attr attr;
	int ret = 0;
	if(strlen(path) == 1)
	{   // 如果是根节点
		attr.size = 0;
		attr.isDirectories = root->isDirectories;
		attr.timeLastModified = root->timeLastModified;
	}
	else{
        // 如果是普通节点
        ret = dhmpGetAttr(path,&attr);
    }
		
	if(ret < 0)
		return -2;
	if(attr.isDirectories == 1)
	{
		stbuf->st_mode = S_IFDIR | 0666;        // 如果路径指定的是目录,则要设置_S_IFDIR位， 后面在模上权限
		stbuf->st_size = 0;
	}
	else
	{
		stbuf->st_size = attr.size;
		stbuf->st_mode = S_IFREG | 0777;
	}
    // 疑问：为什么不把真实从dhmpGetAttr返回的属性赋值给stbuf
    stbuf->st_nlink = 1;            /* Count of links, set default one link. */
    stbuf->st_uid = 0;              /* User ID, set default 0. */
    stbuf->st_gid = 0;              /* Group ID, set default 0. */
    stbuf->st_rdev = 0;             /* Device ID for special file, set default 0. */
    stbuf->st_atime = 0;            /* Time of last access, set default 0. */
    stbuf->st_mtime = attr.timeLastModified; /* Time of last modification, set default 0. */
    stbuf->st_ctime = 0;            /* Time of last creation or status change, set default 0. */
	return 0;

}


/** Read directory
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 */
static int dhmp_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flag)
{

	struct inode_list list,*tmp;
	uint32_t i;
	struct stat st;
	dhmpReadDir(path,&list); // 返回该路径下的所有文件，其以inode_list的链表形式返回，inode_list仅仅包含了文件名和是否是文件夹的信息
	tmp = &list;
	if(tmp->isDirectories == -1) return 0;  // 如果被读取的是一个文件不是文件夹或者是空文件夹，返回-1
	for(; tmp != NULL; tmp = tmp->next)
	{
		memset(&st, 0, sizeof(st));
		st.st_mode = (tmp->isDirectories == 1) ? S_IFDIR : S_IFMT;
		if (filler(buf, tmp->filename, &st, 0,0))   // 这个是fuse自带的接口，将获得的文件名加入到buf中
			break;
	}
	return 0;
}



static int dhmp_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	res = dhmpMknod(path);
	if(res < 0)
		return -1;  // 创建失败
	else
		return 0;
}
static int dhmp_fs_mkdir(const char *path, mode_t mode)
{
	int res = dhmpCreateDirectory( path);
	if(res < 0)
		return -1;
	else
		return 0;
}
static int dhmp_fs_rmdir(const char *path)
{
	int res = dhmpDelete(path);
	if(res < 0)
		return -2;
	else 
		return 0;
}


static int dhmp_fs_unlink(const char *path)
{
	int res = dhmpDelete(path);
	if(res < 0)
		return -2;
	else
		return 0;
}


static int dhmp_fs_rename(const char *from, const char *to, unsigned int flags)
{
	char filename[FILE_NAME_LEN],dirname[FILE_NAME_LEN],toname[FILE_NAME_LEN];
	int len = strlen(from);
	strcpy(filename,from);
	filename[len] = '/'; filename[len+1] = 0;
	struct inode* head = get_father_inode(filename);
	if(head == NULL) return -17;
	deal(to,dirname,toname);
	strcpy(head->filename, toname);
	return 0;
}

static int dhmp_fs_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
	return 0;
}

static int dhmp_fs_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
	return 0;
}

static int dhmp_fs_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
	return 0;
}


static int dhmp_fs_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int dhmp_fs_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
DEBUG("dhmp_fs_read");
	DEBUG_INT(size);
	DEBUG(path);
	DEBUG_INT(offset);
	DEBUG_END();

	char *bbuf = malloc(size);
	char filename[FILE_NAME_LEN];
	int len = strlen(path);
	strcpy(filename,path);
	filename[len] = '/'; filename[len+1] = 0;
	struct inode* head = get_father_inode(filename);
	if(head == NULL || head->isDirectories == 1) return -1; // 如果filename不存在或者是目录返回-1

	struct context * cnt = head->context;
	if(cnt == NULL) return 0;   // 文件为空
	off_t read_offset = offset;
	int i = read_offset / CHUNK_SIZE;   // 需要从第i个chunk开始读
	while(i > 0)
	{
		if(cnt == NULL || cnt -> size  != CHUNK_SIZE){ DEBUG("dhmp_fs_read error");return 0;}
		cnt = cnt->next;    // 移动cnt至第i个chunk
		i--;
	}
	read_offset = read_offset % CHUNK_SIZE; // 需要从第i个chunk的多少偏移位置开始读
	size_t read_size = 0,un_read_size = size;   // read_size已经读了的长度，un_read_size还未读的长度
	while(un_read_size > 0)
	{
		if(cnt == NULL) {DEBUG("dhmp_fs_read eror2");break;}
		if(un_read_size >= cnt->size - read_offset){    // 如果需要读的长度超过起始chunk偏移量之后到chunk结束剩下的长度
            // dhmp_fs_read_from_bank（被读chunk的index, 写bbuf的偏移量， 需要读的长度， 读chunk的偏移量）
			dhmp_fs_read_from_bank( cnt->chunk_index, bbuf + read_size, (cnt->size - read_offset), read_offset);
			read_size += cnt->size - read_offset;
			un_read_size = un_read_size - (cnt->size - read_offset);
			read_offset = 0;
		}
		else
		{   // 把剩下的都读出来
			dhmp_fs_read_from_bank( cnt->chunk_index, bbuf + read_size , un_read_size, read_offset);
			read_size += un_read_size;
			un_read_size = 0;
		}
		cnt = cnt->next;
	}
	bbuf[read_size] = 0;
	memcpy(buf,bbuf,read_size);
	free(bbuf);
DEBUG("dhmp_fs_read over");
DEBUG_INT(size);
    DEBUG(path);
	DEBUG_INT(offset);
	DEBUG_INT(read_size);
	DEBUG_END();
	return read_size;
}

static int dhmp_fs_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
DEBUG("dhmp_fs_write");
        DEBUG_INT(size);
        DEBUG(path);
	DEBUG_INT(offset);
        DEBUG_END();
	int res;
	char filename[FILE_NAME_LEN];
	int len = strlen(path);
	strcpy(filename,path);
	filename[len] = '/'; filename[len+1] = 0;
	struct inode* head = get_father_inode(filename);
	if(head == NULL || head->isDirectories == 1) return -1;
	struct context * cnt = head->context;
	if(cnt == NULL)
	{
		head->context = malloc(sizeof(struct context));
		cnt = head->context;
		cnt->size = 0;
		cnt->chunk_index = getFreeChunk();
		cnt->next = NULL;
	}
	off_t write_offset = offset;
	int i = write_offset / CHUNK_SIZE;	// 需要跳过多少个chunk
	write_offset = write_offset % CHUNK_SIZE;
	DEBUG_INT(i);
	DEBUG_INT(write_offset);
	DEBUG("before headsize= ");
	DEBUG_INT(head->size);
	DEBUG_END();
	while(i > 0)
	{
		if(cnt == NULL) return 0;
		if(cnt -> size  != CHUNK_SIZE) return 0;
		i--;
		if(i == 0 && write_offset == 0 && cnt->next == NULL)
		{
			// 如果write_offset恰好为0，且已经遍历到chunk list的末尾，则需要重新开一个新的chunk
			cnt->next = malloc(sizeof(struct context));
			cnt->next->size = 0;
			cnt->next->chunk_index = getFreeChunk();
			cnt->next->next = NULL;
		}
		cnt = cnt->next;
	}
	
	DEBUG("dhmp_fs_write2");
        DEBUG_INT(cnt->chunk_index);
        DEBUG_INT(write_offset);
		DEBUG_END();
	size_t write_size = 0,un_write_size = size;
	while(un_write_size > 0)
	{
		if(un_write_size >= (CHUNK_SIZE - write_offset)){
			// 将当前chunk写满
			// dhmp_fs_write_to_bank（被写入的chunk的index， 缓冲区偏移量， 被写入的大小， 被写的chunk偏移量）
			dhmp_fs_write_to_bank(cnt->chunk_index, buf + write_size, (CHUNK_SIZE - write_offset), write_offset);
			write_size += (CHUNK_SIZE - write_offset);
			un_write_size = un_write_size - (CHUNK_SIZE - write_offset);
			//head->size += (CHUNK_SIZE - write_offset);
			head->size = head->size - cnt->size + CHUNK_SIZE;
			write_offset = 0;
			cnt->size = CHUNK_SIZE;
			// 如果耗尽了chunk，且还有未写的数据，则继续分配一个新的chunk
			if(cnt->next == NULL && un_write_size > 0)
			{	
				cnt->next = malloc(sizeof(struct context));
				cnt->next->size = 0;
				cnt->next->chunk_index = getFreeChunk();
				cnt->next->next = NULL;
			}
		}
		else
		{
			// 将剩下的未写入数据都写进去
			dhmp_fs_write_to_bank(cnt->chunk_index, buf + write_size , un_write_size, write_offset);
			write_size += un_write_size;
			if(cnt->size != CHUNK_SIZE)
			{
			//			cnt->size += un_write_size;
			//			head->size += cnt->size;
				// 追加写
				head->size = head->size - cnt->size;
				cnt->size = un_write_size + write_offset;
				head->size += cnt->size;
			}
			un_write_size = 0;
		}
		cnt = cnt->next;
	}
	DEBUG("after headsize= ");
        DEBUG_INT(head->size);
	DEBUG("dhmp_fs_write over");
	DEBUG_INT(size);
        DEBUG(path);
	DEBUG_INT(offset);
	 DEBUG_END();
	return size;
}

static int dhmp_fs_statfs(const char *path, struct statvfs *stbuf)
{
	stbuf->f_bsize = BANK_SIZE;
	return 0;
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

int main(int argc, char *argv[])
{
dhmp_local_buf = malloc(BANK_SIZE+1);
	fp = fopen("./fuse_logs.txt", "wb+");
	printf("dhmpmain\n");
	// 用户态程序调用fuse_main() （lib/helper.c）
	int ret = fuse_main(argc, argv, &dhmp_fs_oper, NULL);
	// #ifdef DHMP_ON
	// int init_bank_i;
	// for(init_bank_i = 0;init_bank_i < BANK_NUM; init_bank_i++)
	// 	dhmp_free(bank[init_bank_i]);
	// dhmp_client_destroy();
	// DEBUG("dhmp_client_destroy!");
	// DEBUG_END();
	// #endif
	return ret;
}


