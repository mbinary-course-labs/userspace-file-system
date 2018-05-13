# Simple Filesystem
> An implementation of userspace filesystem using fuse interfaces

## Function
- [x] view attr of files and the root directory
- [x] read file
- [x] write file
- [ ] directory operations ( to do )
## Design
### metadata of fs
* TOTAL SIZE: 4G
* BLOCKSIZE: 4KB
* BLOCKNR  : 4G/4KB
* NAME_LENGTH: 256
* block_no_t: int  (the type of block_no)
* Each block has a block_no at the begining 
### super block
block 0( aka mem[0]) is the super block.

It stores the num of used_blocks and the num of the latest used block.

The latedt used block is exactly the head block of the node-chain, so we can use this to visit all entry nodes.
### filenode 
metadata of each entry( dir or file) stored in mem blocks 
including : 
* int block_no  : num of mem block
* int content   : is the value of the first mem block_no of the files's content( for dir, it's -1)
* stat st       : st of each entry
* filenode\* next: next node
* char name[256]: entry name 

### Free space allocation algorithm 
`next fit`

## Resource
- [binary file](oshfs)
- [test file](src/test.sh)
```shell
gcc -D_FILE_OFFSET_BITS=64 -o oshfs oshfs.c -lfuse 
cd mountpoint
ls -al
echo helloworld > testfile
ls -l testfile # 查看是否成功创建文件
cat testfile # 测试写入和读取是否相同
dd if=/dev/zero of=testfile bs=1M count=2000
ls -l testfile # 测试2000MiB大文件写入
dd if=/dev/urandom of=testfile bs=1M count=1 seek=10
ls -l testfile # 此时应为11MiB
dd if=testfile of=/dev/null # 测试文件读取
rm testfile
ls -al # testfile是否成功删除
```
## Vision
![](src/fs.png)

## Licence
[MIT](LICENCE)
