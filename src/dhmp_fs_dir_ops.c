
#include "../include/dhmp_fs_dir_ops.h"
#include "../include/dhmp_fs_index.h"
#include "../include/dhmp_fs_logs.h"

int dhmpCreateDirectory(const char * path);
int dhmpGetAttr(const char *path,struct attr *attr);
inode_list * dhmpReadDir(const char *path);
int dhmpMknod(const char * path);
void deal(const char *path,char * dirname,char * filename);
inode * get_father_inode(char *dirname);
void dhmpFreeInode(inode * head);
void dhmpDeleteALL(struct inode *head);
// int dhmpDelFromInode(struct inode *head,char * filename);
int dhmpDelete(const char *path);
inode * search_d_hash(const char * path);

void add_to_father(inode * father, inode * now)
{
	inode * tmp = father->son;
	father -> son = now;
	now -> bro = tmp;
	father->son_nums++;
}

void del_from_father(inode * father , inode *del)
{
	//FUSE_INFO_LOG("In del_from_father father is : %s del is: %s", father->filename, del->filename);
	if(father->son == NULL)
	{
		return;
	}
	if(father->son == del)
	{
		father->son = father->son->bro;
		father->son_nums--;
		return;
	}
	inode* now = father->son->bro;
	inode* bef = father->son;
	while (now != NULL)
	{
		if(now == del)
		{
			bef->bro = now->bro;
			father->son_nums--;
			break;
		}
		bef = now;
		now = now->bro;
	}
	//FUSE_INFO_LOG("del_from_father over");
}

int dhmpCreateDirectory(const char * path)
{

	if(strlen(path) == 1) return 0;
	char filename[FILE_NAME_LEN], 	dirname[FILE_NAME_LEN];
	deal(path,dirname,filename);

	inode * father = search_d_hash(dirname);
	if(father == NULL)
		return -1;
	if(father->isDirectories == 0)
		return -1;

	inode* now = father->d_hash->get(father->d_hash, filename);
	if(now != NULL){
		return  -1;	// 检查是否有重名的文件，如果有返回-1
	} 
	now = get_free_inode();
	now -> isDirectories = 1;
	now -> d_hash = createHashMap(NULL, NULL);		// 如果是目录，则需要初始化该目录的d_hash
	strcpy(now->filename,filename);
	pthread_mutex_lock(&father->file_lock);

	// list_add_tail(&now->d_bro, &father->d_son);					// 将该目录的inode加入到他的父亲节点的儿子列表中
	add_to_father(father, now);
	father->d_hash->put(father->d_hash, now->filename, now);		// 同时将其加入到父亲的d_hash中
	pthread_mutex_unlock(&father->file_lock);
//FUSE_INFO_LOG("dhmpCreateDirectory add: %s to father: %s", now->filename, father->filename);
	return 1;
}

// change
int dhmpMknod(const char * path)
{
//FUSE_INFO_LOG("In dhmpMknod, path is %s", path);
	struct attr attr;
	if(strlen(path) == 1) return 0;
	char filename[FILE_NAME_LEN],	dirname[FILE_NAME_LEN];
	deal(path, dirname, filename);    // 将文件所在目录和文件名进行分割

	// struct inode * father = get_father_inode(dirname);  // 获得目录的inode

	inode * father = search_d_hash(dirname);

	if(father == NULL){
		FUSE_ERROR_LOG("EROrr path to mknod, no father %s 's inode!\n", father->filename);
		return -1;
	}

	inode* now = father->d_hash->get(father->d_hash, filename);
	if(now != NULL){
		// 检查是否有重名的文件，如果有返回-1
		return  -1;
	} 

	now = get_free_inode();
	strcpy(now->filename, filename);

	pthread_mutex_lock(&father->file_lock);

	father->d_hash->put(father->d_hash, now->filename, now);	// 将该inode加入到父节点的dhash中
	
	// list_add_tail(&now->d_bro, &father->d_son);		// 将该文件的inode加入到他的父亲节点的儿子列表中
	add_to_father(father, now);

	pthread_mutex_unlock(&father->file_lock);

//FUSE_INFO_LOG("dhmpMknod add: %s to father: %s", now->filename, father->filename);
	return 1;
}

inode * search_d_hash(const char * path)
{
	
	char name[FILE_NAME_LEN];
	memset(name, 0, FILE_NAME_LEN);
	// inode * next_inode = (inode *)now_dnode->d_hash->get(now_dnode->d_hash, name);
	inode * now_node = root;
	int i = 0, len = strlen(path);
//FUSE_INFO_LOG("	Search_d_hash Now path is %s, len is %d", path, len);
	int j =0;
	// 跳过根目录
	if(len == 1){
		return now_node;
	}
	for(i=0, j=0 ; i<len ; i++)
	{
		name[j] = path[i];
		if(path[i] == '/' || i == (len - 1))
		{	// 获得了一个完整的文件名
			if(i == 0)	continue;
			if(path[i] == '/')
			{
				name[j] = 0;
			}else
			{
				name[j+1] = 0;
			}
		//FUSE_INFO_LOG("		Now search_d_hash is %s", name);
			inode * next_inode = (inode *) now_node->d_hash->get(now_node->d_hash, name);
			if(next_inode == NULL){
			//FUSE_INFO_LOG("	Not Found %s, return!", name);
				return NULL;
			} // cd /home/gtwang/FUSE/
			now_node = next_inode;
			j=0;
			continue;
		}
		j++;
	}
//FUSE_INFO_LOG("	search_d_hash Found %s, OK!", name);
	return now_node;
}


// head是father，filename是被删除的文件
int dhmpDelFromInode(struct inode *father,char * filename)
{
//FUSE_INFO_LOG("	In dhmpDelFromInode, father is %s, filename is %s", father->filename, filename);
	if(father == NULL || father->isDirectories == 0) {
		// 父目录下没有文件
	//FUSE_INFO_LOG("	In dhmpDelFromInode, father not a dirct ot is empty!");
		return -1;
	}
	// 获得被删除的inode
	inode * to_del = father->d_hash->get(father->d_hash, filename);
	if(to_del == NULL){
		FUSE_ERROR_LOG("	In dhmpDelFromInode, father remove %s is NULL!", filename);
		return -1;
	}

	// 将被删除的inode将其从其父节点的son链表中删除
	pthread_mutex_lock(&father->file_lock);

	del_from_father(father, to_del);

	// list_del(&to_del->d_bro);
	pthread_mutex_unlock(&father->file_lock);

	// 把被删除的inode从他的父目录的dhash中删除
	if(father->d_hash->remove(father->d_hash, filename) == NULL){
		FUSE_ERROR_LOG("	In dhmpDelFromInode, father remove %s is NULL!", filename);
		return -1;
	}

	// 如果被删除的是文件夹，还需要删除其下所有的文件
	if(to_del->isDirectories == 1){
		//FUSE_INFO_LOG("dhmpDeleteALL begin!");
		dhmpDeleteALL(to_del);		// 传入的是子inode链表的头节点
		// FUSE_INFO_LOG("dhmpDeleteALL end!");
	}
		
	// 归还自身的inode结构体
	reset_indoe(to_del);

//FUSE_INFO_LOG("	dhmpDelFromInode sucess!");
	return 1;
}

// 递归遍历，删除该文件夹下所有文件
// deleteall使用链表进行遍历删除，而不是使用dhash
void dhmpDeleteALL(inode *father)
{
	//FUSE_INFO_LOG("In dhmpDeleteALL, father is %s!", father->filename);
	char tmp[1024];
	strcpy(tmp,father->filename );
	if(father == NULL)
	{
		return;
	}
	inode * son = father->son, *next = NULL;
	if(son == NULL)
	{
		return;
	}
	while(son)
	{
		if(son->isDirectories == 1 && son->son_nums !=0)
		{
			dhmpDeleteALL(son);
		}
		next = son->bro;
		reset_indoe(son);
		son = next;
	}
	//FUSE_INFO_LOG("End dhmpDeleteALL, father is %s!", tmp);
}

// 删除目录和文件没什么区别
int dhmpDelete(const char *path)
{
	// 不能删除根目录？
//FUSE_INFO_LOG("DhmpDelete, path is %s", path);
	if(strlen(path) == 1)
		return 0;
	char filename[FILE_NAME_LEN],	dirname[FILE_NAME_LEN];
	deal(path,	dirname,  filename);
	inode * father = search_d_hash(dirname);
	return dhmpDelFromInode(father , filename);
}


// 根据路径名获得文件属性
int dhmpGetAttr(const char *path,struct attr *attr)
{
//FUSE_INFO_LOG("DhmpGetAttr path is : %s",path);
	inode * head = search_d_hash(path);
	if(head == NULL){
		return -1;
	}
	attr->isDirectories = head->isDirectories;
	attr->size = head->size;
	attr->timeLastModified = head->timeLastModified;
	
	return 0;
}

// 作用：将一个文件夹下的所有文件返回
inode_list * dhmpReadDir(const char *path)
{
//FUSE_INFO_LOG("dhmpReadDir %s!", path);
	// char filename[FILE_NAME_LEN];
	int len = strlen(path);
	inode* father;
	if(len != 1)
	{
		// strcpy(filename,  path);
		father = search_d_hash(path);
		if(father == NULL || father->isDirectories == 0)
			return NULL;
	}
	else
		father = root;
    // 如果head是一个文件的化那么son会是null，因此返回的Li->isDirectories也会是-1
    // 如果head是一个目录，那么其会进入该目录，并将该目录下所有的文件都链接到Li->next上
    // 注意：如果head是一个空目录的话，head->son也会是NULL，空文件夹的话和文件是没有区别的
	inode* son = father->son;
	inode_list *Li_head = NULL;
	int count = 0;
	while(son)
	{
		inode_list * Li = (inode_list *) malloc (sizeof(inode_list));
		Li->next = NULL;
		Li->isDirectories = son->isDirectories;
		strcpy(Li->filename, son->filename);
		if(Li_head == NULL){
			Li_head = Li;
		}else{
			inode_list * tp = Li_head->next;
			Li_head->next = Li;
			Li->next = tp;
		}
		son = son->bro;
		count++;
	}
	// FUSE_INFO_LOG("dhmpReadDir is %s ok!");
	return Li_head;
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


 int dhmp_fs_access(const char *path, int mask)
{
	return 0;
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
 int dhmp_fs_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
	// FUSE_INFO_LOG("In dhmp_fs_getattr, path is %s", path);
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
	// FUSE_INFO_LOG("In dhmp_fs_getattr %s sucess!", path);
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
 int dhmp_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flag)
{
//FUSE_INFO_LOG("In dhmp_fs_readdir, path is %s", path);
	uint32_t i;
	struct stat st;
	inode_list * tmp = dhmpReadDir(path); // 返回该路径下的所有文件，其以inode_list的链表形式返回，inode_list仅仅包含了文件名和是否是文件夹的信息
	if(tmp == NULL){
		// FUSE_INFO_LOG("Empty! return ");
		return 0;
	}
	for(; tmp != NULL;)
	{
		// FUSE_INFO_LOG("Read son %s", tmp->filename);
		memset(&st, 0, sizeof(st));
		st.st_mode = (tmp->isDirectories == 1) ? S_IFDIR : S_IFMT;
		if (filler(buf, tmp->filename, &st, 0,0))   // 这个是fuse自带的接口，将获得的文件名加入到buf中
			break;
		
		// FUSE_INFO_LOG("Read son %s is ok!", tmp->filename);
		struct inode_list * tp = tmp;
		tmp = tmp->next;
		free(tp);
	}
	// free(tmp);
//FUSE_INFO_LOG("dhmp_fs_readdir over!");
	return 0;
}



 int dhmp_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	// FUSE_INFO_LOG("In dhmp_fs_mknod, path is %s, mode is %u", path, mode);
	res = dhmpMknod(path);
	if(res < 0)
		return -1;  // 创建失败
	else
		return 0;
}

 int dhmp_fs_mkdir(const char *path, mode_t mode)
{
//FUSE_INFO_LOG("In dhmp_fs_mkdir, path is %s", path);
	int res = dhmpCreateDirectory( path);
	if(res < 0)
		return -1;
	else
		return 0;
}
 int dhmp_fs_rmdir(const char *path)
{
	//FUSE_INFO_LOG("In dhmp_fs_rmdir, path is %s", path);

	int res = dhmpDelete(path);
	if(res < 0)
		return -2;
	else 
		return 0;
}


 int dhmp_fs_unlink(const char *path)
{
	//FUSE_INFO_LOG("In dhmp_fs_unlink, path is %s", path);
	int res = dhmpDelete(path);
	if(res < 0)
		return -2;
	else
		return 0;
}


 int dhmp_fs_rename(const char *from, const char *to, unsigned int flags)
{
	// FUSE_INFO_LOG("IN rename!, form is %s, to is %s", from, to);
	char filename[FILE_NAME_LEN];
	char dirname[FILE_NAME_LEN];
	char toname[FILE_NAME_LEN];
	// int len = strlen(from);
	// strcpy(filename,from);
	// filename[len] = '/'; 
	// filename[len+1] = 0;
	// struct inode* head = get_father_inode(filename);


	deal(from,	dirname,	filename);

	inode* father = search_d_hash(dirname);
	inode* head = (inode*)father->d_hash->get(father->d_hash, filename);
	if(head == NULL) 
		return -17;
	father->d_hash->remove(father->d_hash, filename);
	// FUSE_INFO_LOG("Remove %s from fathe %s", filename, father->filename);
	deal(to,	dirname,	toname);
	// FUSE_INFO_LOG("Remove!, toname is %s", toname);
	strcpy(head->filename, toname);
	father->d_hash->put(father->d_hash, head->filename, head);
	// FUSE_INFO_LOG("Remove is ok");
	return 0;
}

 int dhmp_fs_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
	//FUSE_INFO_LOG("In dhmp_fs_chmod, path is %s, mode is %u", path, mode);
	return 0;
}

 int dhmp_fs_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
	//FUSE_INFO_LOG("In dhmp_fs_chown, path is %s", path);
	return 0;
}

 int dhmp_fs_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
	//FUSE_INFO_LOG("In dhmp_fs_truncate, path is %s, size is %d", path, size);
	return 0;
}


 int dhmp_fs_open(const char *path, struct fuse_file_info *fi)
{
//FUSE_INFO_LOG("Dhmp_fs_open, path is %s", path);
	// fi->direct_io = 1;
	inode * open_file_inode = search_d_hash(path);
	if(open_file_inode == NULL){
		return -1;
	}
	fi->fh = (uint64_t) open_file_inode;
	return 0;
}

 int dhmp_fs_statfs(const char *path, struct statvfs *stbuf)
{
	//FUSE_INFO_LOG("In dhmp_fs_statfs, path is %s", path);
	stbuf->f_bsize = BANK_SIZE;
	return 0;
}