# Simple Filesystem
> An implementation of userspace filesystem using fuse interfaces

## Function
* view attr of files and the root directory
* read file
* write file

## Design
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
