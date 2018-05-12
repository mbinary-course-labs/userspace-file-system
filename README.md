# Simple Filesystem
> An implementation of userspace filesystem using fuse interfaces

## Function
* view attr of files and the root directory
* read file
* write file

## Design
### metadata of fs
* TOTAL SIZE: 4G
* BLOCKSIZE: 4KB
* BLOCKNR  : 4G/4KB
* NAME_LENGTH: 256
* block_no_t: int  (the type of block_no)
* Each block has a block_no at the begining 

### filenode 
metadata of each entry( dir or file) stored in mem blocks 
including : 
* int block_no  : num of mem block
* int content   : is the value of the first mem block_no of the files's content( for dir, it's -1)
* stat st       : st of each entry
* filenode* next: next node
* char name[256]: entry name 

### Free space allocation algorithm 
`next fit`

 
## Vision
![](fs.png)

## Licence
[MIT](LICENCE)
