#!/bin/bash
./del.sh;
./bin/fuse_naive ./temp2 -o noforget;
# ./bin/fuse_naive ./temp


# 限制dirty_bytes（影响Ext4本地write的性能）
./cachePage_sh/change.sh;

start_time=$(date +%s)
cp -rf ./fuse_test_10M.txt ./temp2/
end_time=$(date +%s)
cost_time=$[ $end_time-$start_time ]
echo "FUse + SSD + limit write + time is $(($cost_time/60))min $(($cost_time%60))s"

# 清除pagecache（影响Ext4本地读的性能）
./cachePage_sh/drop_cache.sh

start_time=$(date +%s)
./bin/rtest
end_time=$(date +%s)
cost_time=$[ $end_time-$start_time ]
echo "FUse + SSD + limit read +  time is $(($cost_time/60))min $(($cost_time%60))s"


# cp -rf mongodb/db temp/





# cp -f fuse_test_10M.txt ./temp/fuse_test_10M.txt
# md5sum fuse_test_10M.txt > md5_1
# md5sum ./temp/fuse_test_10M.txt > md5_2
# echo "md5结果差异"
# diff md5_1 md5_2
# echo "二进制结果差异"
# od -t x1 ./temp/fuse_test_10M.txt > x16_result_fuse
# od -t x1 ./fuse_test_10M.txt > x16_result
# diff x16_result x16_result_fuse


# cp -f fuse_test_10M.txt ./temp/fuse_test_10M2.txt
# cp -f fuse_test_10M.txt ./temp/fuse_test_10M3.txt


# cp -rf /home/gtwang/spark/spark/ temp/

# cp -rf ./test/ temp/
