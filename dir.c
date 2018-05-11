/*
#########################################################################
# File : oshfs.c
# Author: mbinary
# Mail: zhuheqin1@gmail.com
# Blog: https://mbinary.github.io
# Github: https://github.com/mbinary
# Created Time: 2018-05-02  23:09
# Description: 
#########################################################################
*/

#define FUSE_USE_VERSION 26
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h>

#define NAME_LENGTH 255
#define BLOCKSIZE 4096
#define BLOCKNR  1048576 //1<<20 =  total/blocksize = 4g/4k

static const size_t size = 4 * 1024 * 1024 * (size_t)1024;
tydef int offset_t  ;
static void *mem[ BLOCKNR];
static struct filenode *root = NULL;
int used_block = 0;

struct filenode {
    char *filename;
    void *content;
	int memnum ;
    struct stat *st;
    struct filenode *next;
};

int node_size = sizeof(struct  filenode);

// metadata of node;  including pointer and content
int header =  sizeof(void *)*4 + sizeof(struct stat) +sizeof(int) ;

static struct filenode *get_filenode(const char *name,struct filenode **prt)
{ 
	if( !root)return NULL;
	if(strcmp(root->filename,name+1)==0)return root;
    struct filenode *node = root;
    while(node->next) {
        if(strcmp(node->next->filename, name + 1) != 0)
            node = node->next;
        else{
			*prt = node;
            return node->next;
		}
    }
    return NULL;
}

void * seek_mem(int block_no,int pos){
	return (void*) ((char*)mem[block_no] + pos);
}
int map_mem(){
  /* get a vacant block and mmap, and init header info*/
	int i = 0;
	for(;i<BLOCKNR;++i){
		if(!mem[i]){
			mem[i] = mmap(NULL, blocksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			*(offset_t *) (seek_mem(i,header)) = 0;
			return i;
		}
	}
	return -1;
}
static struct filenode* create_filenode(const char *filename, const struct stat *st)
{
	int i = map_mem(0);
	if(i==-1) return NULL;
	struct filenode * nd = (struct filenode*)seek_mem(i,0);
    memcpy(nd->filename, filename, strlen(filename) + 1);
    memcpy((char *)seek_mem(i,node_size, st, sizeof(struct stat));
    new->next = root;
    new->content = NULL;
    root = new;
	return root;
}

static void *zfs_init(struct fuse_conn_info *conn)
{
    size_t blocknr = sizeof(mem) / sizeof(mem[0]);
    size_t blocksize = size / blocknr;
    // Demo 1
    for(int i = 0; i <blocknr; i++) {
        mem[i] = mmap(NULL, blocksize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memset(mem[i], 0, blocksize);
    }
    for(int i = 0; i <blocknr; i++) {
        munmap(mem[i], blocksize);
    }
    // Demo 2
    mem[0] = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for(int i = 0; i <blocknr; i++) {
        mem[i] = (char *)mem[0] + blocksize * i;
        memset(mem[i], 0, blocksize);
    }
    for(int i = 0; i <blocknr; i++) {
        munmap(mem[i], blocksize);
    }
    return NULL;
}

static int zfs_mknod(const char *path, mode_t mode, dev_t dev)
{  
	/* when createing a file, if create not impl, call mknod  @fuse.h L368 ,106
	 * this is called for creation of all  non-directory, non-symlink nodes.
     * */
    if(get_filenode(path,NULL)!=NULL) return -EEXIST; //  if exists, 
    struct stat st;
    st.st_mode = S_IFREG | 0644;
    st.st_uid = fuse_get_context()->uid;
    st.st_gid = fuse_get_context()->gid;
    st.st_nlink = 1;
    st.st_size = 0;
    create_filenode(path + 1, &st);
    return 0;
}

static int zfs_open(const char *path, struct fuse_file_info *fi)
{

    return 0;
}


static int zfs_getattr(const char *path, struct stat *stbuf)
{
    int ret = 0;
    struct filenode *node = get_filenode(path,NULL);
    if(strcmp(path, "/") == 0) {
        memset(stbuf, 0, sizeof(struct stat));
		//stbuf->st_nlink = 2;
		//stbuf->st_size=0;
        stbuf->st_mode = S_IFDIR | 0755;
    } else if(node) {
        memcpy(stbuf, node->st, sizeof(struct stat));
    } else { ret = -ENOENT;
    }
    return ret;
}

static int zfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct filenode *node = root;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    while(node) {
        filler(buf, node->filename, node->st, 0);
        node = node->next;
    }
    return 0;
}
static int zfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct filenode *node = get_filenode(path,NULL);
    int ret = size;
    if(offset + size> node->st->st_size)
        ret = node->st->st_size - offset;
    memcpy(buf, node->content + offset, ret);
    return ret;
}


static int zfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    struct filenode *node = get_filenode(path,NULL);
    node->st->st_size = offset + size;
    node->content = realloc(node->content, offset + size);
    memcpy(node->content + offset, buf, size);
    return size;
}

static int zfs_truncate(const char *path, off_t size)
{      // resize    file
    struct filenode *node = get_filenode(path,NULL);
    node->st->st_size = size;
    node->content = realloc(node->content, size);
    return 0;
}
static int zfs_unlink(const char *path)
{ 
    struct filenode *node,**prt = &node;
	node = get_filenode(path,prt);
	if(node==NULL) return -ENOENT;
	if(*prt==NULL){
		root = root->next;
	}else (*prt)->next = node->next;
	struct filenode *p=node->content;
	while(node){ 
		unmap_mem(node->memnum);
		node = p;
		p = p->next;
	}
    return 0;
}

void unmap_mem(  int i){ 
	munmap(  mem[  i],blocksize);
	mem[  i] = NULL;
}
static int zfs_mkdir(const char * path, mode_t mode){
	struct stat * st;
	struct filenode * nd  = create_filenode(path+1,st);
	strcpy(nd->filename,path+1);  // skip the leading '/'
	nd->st->st_mode = mode | S_IFDIR;
	return 0;
	
}

static int zfs_rmdir(const char * path){
	return zfs_unlink(path);	
}

static const struct fuse_operations op = {
    .init = zfs_init,
    .getattr = zfs_getattr,
    .readdir = zfs_readdir, 
    .mknod = zfs_mknod,  
    .open = zfs_open,
    .write = zfs_write,
    .truncate = zfs_truncate,
    .read = zfs_read,
    .unlink = zfs_unlink,
	.mkdir = zfs_mkdir,
	.rmdir = zfs_rmdir,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
