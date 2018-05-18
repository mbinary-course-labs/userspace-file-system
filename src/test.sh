set -x # debug on
gcc -D_FILE_OFFSET_BITS=64 -o zfs  $1 -lfuse 
./zfs -o nonempty  mountpoint
cd mountpoint
cp  ../test.sh .
md5sum  ../test.sh test.sh
diff  -qy ../test.sh test.sh
#mv test.sh  teeeeeeestfile
ls -hl teeeeeeestfile # 查看是否成功创建文件
dd if=/dev/zero of=teeeeeeestfile bs=1M count=50
ls -hl teeeeeeestfile # 测试2000MiB大文件写入
dd if=/dev/urandom of=teeeeeeestfile bs=1M count=1 seek=10
ls -hl teeeeeeestfile # 此时应为11MiB
dd if=teeeeeeestfile of=/dev/null # 测试文件读取
chmod 777 teeeeeeestfile && chown mbinary  teeeeeeestfile 
ls -hl teeeeeeestfile
cd ..
umount -l mountpoint
set +x # debug off
