#gcc  -D_FILE_OFFSET_BITS=64 -o out $1 -lfuse
#sudo umount -l mountpoint
./oshfs mountpoint
sudo cd mountpoint
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
