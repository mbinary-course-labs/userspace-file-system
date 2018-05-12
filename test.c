#include "oshfs.c"
void display(struct filenode *node){
	printf("***** mem[%d] *****\n",node->block_no);
	printf("name: %s\n",node->name);
	printf("dir?: %d\n",S_ISDIR(node->st.st_mode));
	printf("cont: %d\n",node->content);
	printf("atim: %d\n",node->st.st_atime);
	printf("uid : %d\n",node->st.st_uid);
	struct filenode *nd = node->next;
	if(nd!=NULL) printf("next: %s\n",nd->name);
	printf("******************\n");
}


void _get_filenode(const char * path){
	printf("in get filenode\n"); 
	struct filenode * nd ,tmp;
	nd = get_filenode(path,&tmp);
	display(nd);
}

void _create_filenode(const char * path){
	printf("creating %s\n",path);
	struct stat st;
	struct filenode *nd = create_filenode(path,&st);
}


void print_nodes(){
	struct filenode * nd = root;
	while(nd){
		display(nd);
		nd = nd->next;
	}
}

void _mknode(const char * path){
	zfs_mknod(path,0,1);
	printf("after mknode %s\n",path);
	print_nodes();
}


void _unlink( const char * path){
	zfs_unlink(path);
	printf("after unlinking %s\n",path);
	print_nodes();
}

void _read(const char *path,size_t size,off_t offset){
	printf("reading [%s]  for [%d] bytes from [%d]\n",path,size,offset);
	char buf[200];
	struct fuse_file_info fi;
	zfs_read(path,buf,size,offset,&fi);
	printf("buf: %s\n",buf);
}

void _write(const char *path,const char *buf,size_t size, off_t offset){
	printf("writing [%s] for [%d] bytes from [%d]\n",path,size,offset);
	struct fuse_file_info fi;
	zfs_write(path,buf,size,offset, &fi);
}

int main(){

	_create_filenode("/haha");
	_create_filenode("/test");
	//_get_filenode("/test");
	_mknode("/mbinary");
	char p[] = "I am happy today! Hello everyone";
	_write("/test",p,sizeof(p),0);
	_get_filenode("/test");
	_read("/test",sizeof(p),0);
	_write("/test","zhq",3,sizeof(p)-1);
	//_read("/test",sizeof(p)+3,3);
	_unlink("/test");
	_unlink("/mbinary");

	return 0;
}
