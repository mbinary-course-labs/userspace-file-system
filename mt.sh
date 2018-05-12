gcc -D_FILE_OFFSET_BITS=64 -o test  $1 -lfuse && ./test mountpoint
