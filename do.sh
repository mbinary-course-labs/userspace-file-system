gcc  -D_FILE_OFFSET_BITS=64 -o oshfs oshfs.c -lfuse
sudo umount -l mountpoint
./oshfs mountpoint
