gcc -D_FILE_OFFSET_BITS=64 -o oshfs oshfs.c -lfuse 

set -x # debug on

cd mountpoint
ls -al
touch test
mv test testfile
ls -l testfile # 查看是否成功创建文件
echo hello > testfile
cat testfile # 测试写入和读取是否相同
chmod 777 testfile && chown mbinary  testfile 
ls -l testfile
dd if=/dev/zero of=testfile bs=1M count=2000
ls -l testfile # 测试2000MiB大文件写入
dd if=/dev/urandom of=testfile bs=1M count=1 seek=10
ls -l testfile # 此时应为11MiB
dd if=testfile of=/dev/null # 测试文件读取
rm testfile
ls -al # testfile是否成功删除

set +x # debug off
