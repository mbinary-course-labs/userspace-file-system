/*
#########################################################################
# File : oshfs.c
# Author: mbinary
# Mail: zhuheqin1@gmail.com
# Blog: https://mbinary.github.io
# Github: https://github.com/mbinary
# Created Time: 2018-05-10  23:09
# Description:
#    Simple userspace filesystem using fuse interface
#    Currently, I implemented operation on files such as:
#    read, write,   view attributes
#########################################################################
*/

#define FUSE_USE_VERSION 26
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fuse.h>
#include <sys/mman.h>

#define NAME_LENGTH 256
#define BLOCKSIZE 4096    // 4kb
#define BLOCKNR  1048576 //1<<20 =  total/blocksize = 4g/4k
#define  CONTENT_SIZE  (4096 -  sizeof(block_no_t)) 


typedef int block_no_t;
static void *mem[BLOCKNR+1];
static struct filenode *ROOT ;
static block_no_t *USED_BLOCK , *LAST_BLOCK;

/* filenode : metadata of each entry( dir or file) stored in mem blocks 
 * filenode.content: is the value of the first mem block_no of the files's content( for dir, it's -1)
 * Free space allocation algorithm:  next fit
 */
struct filenode {
    block_no_t block_no ;
    block_no_t  content;
    struct stat st;
    struct filenode *next;
    char name[NAME_LENGTH];
};


static block_no_t map_mem(block_no_t i){
  /* get a vacant block and mmap, and init header info*/
    for(i=i%BLOCKNR;i<BLOCKNR;++i){
        if(!mem[i]){
            *LAST_BLOCK = i;
            *USED_BLOCK +=1;
            mem[i] = mmap(NULL, BLOCKSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            *(block_no_t*) mem[i] = -1;
            return i;
        }
    }
    return -1;
}
void unmap_mem(block_no_t  i){ 
    munmap(mem[i],BLOCKSIZE);
    *USED_BLOCK -=1;
    mem[i] = NULL;
}

void unmap_chain_block(block_no_t no){
    block_no_t suc_block ;
    while(no!=-1){
        suc_block = *(block_no_t*) mem[no];
        unmap_mem(no);
        no = suc_block;
    }
}

void * get_next_block(block_no_t* block_no){
    *block_no = *(block_no_t*) mem[*block_no];
    return mem[*block_no]+sizeof(block_no_t);
}

void * seek_mem(block_no_t* block_no,off_t *offset){
    /*  find the begin block to read according to offset */
    block_no_t former_block = * block_no;
    while(*offset>CONTENT_SIZE){
        former_block = *block_no;
        *block_no = *(block_no_t*) mem[*block_no];
        *offset -= CONTENT_SIZE;
    }
    /* when writing, seeking offset, if there is no mem allocated , then allocate, 
     * IN func zfs_write, it ensure the block_no won't be the filenode->content 
     **/
    if(*block_no ==-1){
        *block_no = map_mem(*LAST_BLOCK);
        if(*block_no==-1) return NULL;
        *(block_no_t*) mem[former_block] = *block_no;
    }
    return (char*)mem[*block_no] + *offset+ sizeof(block_no_t);
}

static struct filenode *get_filenode(const char *name,struct filenode **prt){ 
    if(!ROOT)return NULL;
    if(strcmp(ROOT->name,name+1)==0){      // name+1 :  skip the leading "/"
        if(prt!=NULL)*prt= NULL;
        return ROOT;
    }
    struct filenode *node = ROOT;
    while(node->next) {
        if(strcmp(node->next->name, name + 1) != 0)
            node = node->next;
        else{
            if(prt!=NULL) *prt = node;
            return node->next;
        }
    }
    return NULL;
}

static struct filenode* create_filenode(const char *name, const struct stat *st){
    block_no_t i = map_mem(*LAST_BLOCK);
    if(i==-1) return NULL;
    ++name; // skip the leading "/"
    int namelen = sizeof(name);
    if(namelen>NAME_LENGTH) namelen = NAME_LENGTH;
    struct filenode * nd = (struct filenode*)mem[i];
    memcpy(nd->name, name,namelen);
    memcpy(&nd->st , st, sizeof(struct stat));
    nd->block_no = i;  //block_no
    nd->content=-1; //content pointer
    nd->next =ROOT; //next pointer
    ROOT = nd;
    return ROOT;
}

static void *zfs_init(struct fuse_conn_info *conn){       
	/* set all block  free*/
    memset(mem,0,sizeof(void *) * BLOCKNR);	
	ROOT=NULL;
	/* set no.(bLOCKNR+1) block as the super block
	 * storing info such as used_block(num), last_block(allocated)
	 * */
	void * p_mem = mem[map_mem(BLOCKNR)];
	USED_BLOCK = (block_no_t*)((char*)p_mem+sizeof(block_no_t));
	LAST_BLOCK =(block_no_t*)((char*)p_mem+sizeof(block_no_t)*2);
	*USED_BLOCK = *LAST_BLOCK=0;
}
 

static int zfs_mknod(const char *path, mode_t mode, dev_t dev){  
    /* when createing a file, if create not impl, call mknod  @fuse.h L368 ,106
     * this is called for creation of all  non-directory, non-symlink nodes.
     * */
    struct stat st;
    st.st_mode =mode| S_IFREG | 0644;
    st.st_uid = fuse_get_context()->uid;
    st.st_gid = fuse_get_context()->gid;
    st.st_nlink = 1;
    st.st_size = 0;
    st.st_blksize = BLOCKSIZE;
    st.st_atime = time(NULL);
    st.st_mtime = time(NULL);
    st.st_ctime = time(NULL);
    struct filenode * nd = create_filenode(path , &st);
    if(nd==NULL) return -ENOSPC;
    return 0;
}

static int zfs_open(const char *path, struct fuse_file_info *fi){
    return 0;
}

static int zfs_getattr(const char *path, struct stat *stbuf){
    struct filenode *node = get_filenode(path,NULL);
    memset(stbuf,0,sizeof(struct stat));
    if(strcmp(path, "/") == 0){  // neccesary
        stbuf->st_mode = S_IFDIR | 0755;   
        return 0;
    }else if(node) {
        memcpy(stbuf,&node->st, sizeof(struct stat));
        return 0;
    } 
    else return  -ENOENT;
}

static int zfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    // to polish for dir operations
    struct filenode *node = ROOT; 
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    while(node) {
        filler(buf, node->name, &node->st, 0);
        node = node->next;
    }
    return 0;
}

static int zfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    char * buf = buffer;
    struct filenode *node = get_filenode(path,NULL);
    node->st.st_atime = time(NULL);
    if(offset>node->st.st_size) return 0; 
    size_t ret = size;
    if(offset + size> node->st.st_size)
        ret = node->st.st_size - offset;
    block_no_t tmp= node->content, *block_no= &tmp;
    void * p_mem = seek_mem(block_no,&offset);  // new offset is within cur block 
    size_t cur_block_left = CONTENT_SIZE - offset;
    if(cur_block_left > ret){
        memcpy(buf,(char *)p_mem, ret);
        return ret;
    }
    memcpy(buf, (char*)p_mem, cur_block_left);
    buf+=cur_block_left;
    size_t left = ret- cur_block_left;
    while(left>CONTENT_SIZE){
        memcpy(buf,(char*)get_next_block(block_no),CONTENT_SIZE);
        buf+=CONTENT_SIZE;
        left-=CONTENT_SIZE;
    }
    memcpy(buf,(char*)get_next_block(block_no),left);
    return ret;
}


static int zfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    char *buf =(char*) buffer;
    struct filenode *node = get_filenode(path,NULL);
    if(offset > node->st.st_size+1) return -ENOENT;
    node->st.st_size = offset + size;
    node->st.st_mtime = time(NULL);
    node->st.st_atime = time(NULL);
    if(node->content==-1) { // first write 
        node->content = map_mem(*LAST_BLOCK);
        if(node->content==-1)return -ENOSPC;
    }
    block_no_t tmp  = node->content, *block_no = &tmp;
    void * p_mem  = seek_mem(block_no,&offset);
    /* after calling seek_mem, now offset is within one block */
    block_no_t num = (offset+size)/CONTENT_SIZE;  //think twice
    node->st.st_blocks += num;
    if(num > (BLOCKNR - *USED_BLOCK)) return -ENOSPC;
    size_t cur_block_left = CONTENT_SIZE - offset;
    if(size<cur_block_left){
        memcpy(p_mem, buf, size);
        return size;
    }
    memcpy(p_mem, buf, cur_block_left);
    buf+= cur_block_left;
    size_t left = size-cur_block_left ;
    while(left>CONTENT_SIZE){
        *(block_no_t*) mem[*block_no]  = map_mem(*LAST_BLOCK);// new block 
        memcpy((char*)get_next_block(block_no),buf,CONTENT_SIZE);
        buf+=CONTENT_SIZE;
        left-=CONTENT_SIZE;
    }
    *(block_no_t*) mem[*block_no]  = map_mem(*LAST_BLOCK);
    memcpy((char*)get_next_block(block_no),buf,left);
    return size;
}

static int zfs_truncate(const char *path, off_t size){
    /* resize   file*/
    struct filenode *node = get_filenode(path,NULL);
    if(size > node->st.st_size) return 0;
    node->st.st_blocks = (size+CONTENT_SIZE-1) / CONTENT_SIZE; //ceiling(size)
    node->st.st_size = size;
    block_no_t tmp = node->content, *block_no = &tmp ;
    seek_mem(block_no,&size); // size is the offset 
    /* unmap exceeded blocks */
    tmp = *(block_no_t*) mem[*block_no];
    unmap_chain_block(tmp);
    return 0;
}
static int zfs_unlink(const char *path){ 
    struct filenode *node,*prt_,**prt = &prt_;
    node = get_filenode(path,prt);
    if(node==NULL) return -ENOENT;
    if(*prt==NULL){
        ROOT = ROOT->next;
    }else (*prt)->next = node->next;
    unmap_chain_block(node->content);
    return 0;
}


static int zfs_mkdir(const char * path, mode_t mode){
    struct stat  st;
    st.st_uid = fuse_get_context()->uid; 
    st.st_gid = fuse_get_context()->gid; 
    st.st_mode = mode| S_IFDIR | 0755;
    st.st_nlink = 2;
    st.st_blksize = BLOCKSIZE;
    st.st_atime = time(NULL);
    st.st_mtime = time(NULL);
    st.st_ctime = time(NULL);
    st.st_size = 0;
    struct filenode * nd  = create_filenode(path,&st);
    if(nd==NULL) return -ENOSPC;
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
    //.mkdir = zfs_mkdir,
    //.rmdir = zfs_rmdir,
};

int main(int argc, char *argv[]){
    return fuse_main(argc, argv, &op, NULL);
}
