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
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fuse.h>
#include <sys/mman.h>

#define NAMELEN 256
#define BLOCKSIZE 4096    // 4kb
#define BLOCKNR  1048576 //1<<20 =  total/blocksize = 4g/4k
#define  CONTENT_SIZE  (4096 -  sizeof(block_no_t)) 

typedef int block_no_t;

/* entry_node : metadata of each entry( dir or file) stored in mem blocks 
 * content: the value of the first mem block_no of the files's content( for dir, it's -1)
 * Free space allocation algorithm:  next fit
 */
struct entry_node 
{
    block_no_t block_no ;
    block_no_t  content;
    struct stat st;
    struct entry_node *next;
    char name[NAMELEN];
};

/* first block, stored in mem[0]
 * namelen: the max length of name
 * */
struct super_node
{
    block_no_t block_no;
    block_no_t used_block;
    block_no_t last_block;
    block_no_t blocknr;
    unsigned  namelen;
    unsigned  blocksize;
    struct entry_node * next;
};

static void *mem[BLOCKNR];
static block_no_t *used_block , *last_block;
static struct super_node * super;

struct data_node
{
    block_no_t block_no;
    char content[CONTENT_SIZE];
};


static block_no_t map_mem(block_no_t i)
{
  /* get a vacant block and mmap, and init header info*/
    for(i=i%BLOCKNR;i<BLOCKNR;++i){
        if(!mem[i]){
            *last_block = i;
            *used_block +=1;
            mem[i] = mmap(NULL, BLOCKSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            *(block_no_t*) mem[i] = -1;
            return i;
        }
    }
    return -1;
}
void unmap_mem(block_no_t  i)
{ 
    munmap(mem[i],BLOCKSIZE);
    *used_block -=1;
    mem[i] = NULL;
}

void unmap_chain_block(block_no_t no)
{
    block_no_t suc_block ;
    while(no!=-1){
        suc_block = *(block_no_t*) mem[no];
        unmap_mem(no);
        no = suc_block;
    }
}

void * get_next_block(block_no_t* block_no)
{
    /* for file content: get next chain-block */
    *block_no = *(block_no_t*) mem[*block_no];
    return mem[*block_no]+sizeof(block_no_t);
}

static struct entry_node *get_entry_node(const char *name,struct entry_node **prt)
{
    if(!super->next)return NULL;
    if(strcmp(super->next->name,name+1)==0){      // name+1 :  skip the leading "/"
        if(prt)*prt= NULL;
        return super->next;
    }
    struct entry_node *node = super->next;
    while(node->next) {
        if(strcmp(node->next->name, name + 1) != 0)
            node = node->next;
        else{
            if(prt) *prt = node;
            return node->next;
        }
    }
    return NULL;
}

static struct entry_node* create_entry_node(const char *name, const struct stat *st)
{
    block_no_t i = map_mem(*last_block);
    if(i==-1) return NULL;
    ++name; // skip the leading "/"
    /*wrong:   int namelen = sizeof(name);  */
    int namelen = strlen(name);
    if(namelen>NAMELEN) namelen = NAMELEN;
    struct entry_node * nd = (struct entry_node*)mem[i];
    memcpy(nd->name, name,namelen);
    memcpy(&nd->st , st, sizeof(struct stat));
    nd->block_no = i;  //block_no
    nd->content=-1; //content pointer
    nd->next =super->next; //next pointer
    super->next = nd;
    return super->next;
}

static void *zfs_init(struct fuse_conn_info *conn)
{
    /* set all block  free*/
    memset(mem,0,sizeof(void *) * BLOCKNR);
    /* set block 0 as the super block
     * storing info such as used_block(num), last_block(allocated)
     * */
    mem[0]= mmap(NULL, BLOCKSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    super = mem[0];
    last_block = &(super->last_block);
    used_block = &(super->used_block);
    *used_block =1;
    *last_block=0;
    super->next = NULL;
    super->namelen = NAMELEN;
    super->blocknr = BLOCKNR;
    super->blocksize = BLOCKSIZE;
    super->block_no = 0;
    return NULL; 
}
 
static int zfs_open(const char *path, struct fuse_file_info *fi)
{
    if(get_entry_node(path,NULL)==NULL) return -ENOENT;
    return 0;
}



static int zfs_mknod(const char *path, mode_t mode, dev_t dev)
{  
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
    struct entry_node * nd = create_entry_node(path , &st);
    if(nd==NULL) return -ENOSPC;
    return 0;
}

static int zfs_chmod(const char *path,mode_t mode)
{
    struct entry_node * node = get_entry_node(path,NULL);
    if(!node) return -ENOENT;
    node->st.st_mode = mode;
    return 0;
}

static int zfs_chown(const char * path, uid_t uid,gid_t gid)
{
    struct entry_node* node = get_entry_node(path,NULL);
    if(!node) return -ENOENT;
    node->st.st_uid = uid;
    node->st.st_gid = gid;
    return 0;
}

static int zfs_rename(const char* old, const char * new)
{
    struct entry_node* node = get_entry_node(old,NULL);
    if(node==NULL)return -ENOENT;
    memcpy(node->name,new,sizeof(new));
    return 0;
}

static int zfs_utimens(const char * path,const struct timespec tv[2])
{
    struct entry_node* node = get_entry_node(path,NULL);
    if(!node) return -ENOENT;
    node->st.st_mtime = node->st.st_atime = tv->tv_sec;
    return 0;
}

static int zfs_getattr(const char *path, struct stat *stbuf)
{
    struct entry_node *node = get_entry_node(path,NULL);
    if(strcmp(path, "/") == 0){  // neccesary
        memset(stbuf,0,sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;   
        stbuf->st_uid = fuse_get_context() -> uid;
        stbuf->st_gid = fuse_get_context() -> gid;
        return 0;
    }else if(node) {
        memcpy(stbuf,&node->st, sizeof(struct stat));
        return 0;
    } 
    else return  -ENOENT;
}

static int zfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    // to polish for dir operations
    struct entry_node *node = super->next; 
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    while(node) {
        filler(buf, node->name, &node->st, 0);
        node = node->next;
    }
    return 0;
}

static int zfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char * buf = buffer;
    struct entry_node *node = get_entry_node(path,NULL);
    node->st.st_atime = time(NULL);
    if(offset>node->st.st_size) return 0; 
    size_t ret = size;
    if(offset + size> node->st.st_size)
        ret = node->st.st_size - offset;
    block_no_t block_no= node->content;
    for(int i=offset/CONTENT_SIZE;i>0;--i){
        block_no = *(block_no_t*)mem[block_no];
    }
    offset %=CONTENT_SIZE;
    void * p_mem = (char*) mem[block_no] + sizeof(block_no_t) + offset; 
    size_t cur_block_left = CONTENT_SIZE - offset;
    if(cur_block_left > ret){
        memcpy(buf,p_mem, ret);
        return ret;
    }
    memcpy(buf,p_mem, cur_block_left);
    buf+=cur_block_left;
    size_t left=ret- cur_block_left;
    for(int i=left/CONTENT_SIZE;i>0;--i){
        memcpy(buf,(char*)get_next_block(&block_no),CONTENT_SIZE);
        buf+=CONTENT_SIZE;
    }
    memcpy(buf,(char*)get_next_block(&block_no),left%CONTENT_SIZE);
    return ret;
}


static int zfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char *buf =(char*) buffer;
    struct entry_node *node = get_entry_node(path,NULL);
    if(offset > node->st.st_size) return -ENOENT;
    if(offset+size > node->st.st_size) node->st.st_size = offset + size;
    node->st.st_mtime = time(NULL);
    node->st.st_atime = time(NULL);
    if(node->content==-1) { // first write 
        node->content = map_mem(*last_block);
        if(node->content==-1)return -ENOSPC;
    }
    block_no_t block_no= node->content;
    for(int i=offset/CONTENT_SIZE-1;i>0;--i){
        block_no = *(block_no_t*)mem[block_no];
    }
    block_no_t *p = mem[block_no];
    if(*p==-1){
        *p= map_mem(*last_block);
    }
    block_no = *p;
    offset %= CONTENT_SIZE; 
    void *p_mem = (char *)mem[block_no] + sizeof(block_no_t) + offset;
    block_no_t num = (offset+size)/CONTENT_SIZE;  //think twice
    node->st.st_blocks += num;
    //*used_block+= num;
    if(num > (BLOCKNR - *used_block)) return -ENOSPC;
    size_t cur_block_left = CONTENT_SIZE - offset; 
    if(size<cur_block_left){
        memcpy(p_mem, buf, size);
        return size;
    }
    memcpy(p_mem, buf, cur_block_left);
    buf+= cur_block_left;
    size_t left = size-cur_block_left ;
    for(int i=left/CONTENT_SIZE;i>0;--i){
        *(block_no_t*) mem[block_no]  = map_mem(*last_block);// new block 
        memcpy((char*)get_next_block(&block_no),buf,CONTENT_SIZE);
        buf+=CONTENT_SIZE;
    }
    *(block_no_t*) mem[block_no]  = map_mem(*last_block);
    memcpy((char*)get_next_block(&block_no),buf,left%CONTENT_SIZE);
    return size;
}

static int zfs_truncate(const char *path, off_t size)
{
    /* resize   file*/
    struct entry_node *node = get_entry_node(path,NULL);
    if(size > node->st.st_size) return 0;
    node->st.st_blocks = (size+CONTENT_SIZE-1) / CONTENT_SIZE; //ceiling(size)
    node->st.st_size = size;
    /* unmap exceeded blocks */
    block_no_t no=node->content;
    for(int i = node->st.st_blocks;i>0;--i){
        no =  *(block_no_t*)mem[no];
    }
    unmap_chain_block(no);
    return 0;
}
static int zfs_unlink(const char *path)
{
    struct entry_node *node,*prt_,**prt = &prt_;
    node = get_entry_node(path,prt);
    if(node==NULL) return -ENOENT;
    if(*prt==NULL){
        super->next = super->next->next;
    }else (*prt)->next = node->next;
    unmap_chain_block(node->content);
    return 0;
}


static int zfs_mkdir(const char * path, mode_t mode)
{
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
    struct entry_node * nd  = create_entry_node(path,&st);
    if(nd==NULL) return -ENOSPC;
    return 0;
}

static const struct fuse_operations op = 
{
    .init = zfs_init,
    .getattr = zfs_getattr,
    .readdir = zfs_readdir, 
    .mknod = zfs_mknod,  
    .open = zfs_open,
    .write = zfs_write,
    .truncate = zfs_truncate,
    .read = zfs_read,
    .unlink = zfs_unlink,
    .chmod = zfs_chmod,
    .chown = zfs_chown,
    .rename = zfs_rename,
    .utimens = zfs_utimens,
    //.mkdir = zfs_mkdir,
    //.rmdir = zfs_rmdir,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}
