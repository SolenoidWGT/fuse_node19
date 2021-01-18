#!/bin/bash
./del.sh;
./bin/fuse ./temp  -o noforget;
# ./bin/fuse_naive ./temp2 -o noforget;
# ./bin/fuse_naive ./temp

# mogodb拷贝
# cp -rf mongodb/db temp/;

# # spark拷贝任务
# start_time=$(date +%s);
# cp -rf ../spark ./temp/;
# end_time=$(date +%s);
# cost_time=$[ $end_time-$start_time ];
# echo "NEW FUse time is $(($cost_time/60))min $(($cost_time%60))s";

# # 写任务
# start_time=$(date +%s);
# cp -rf ./fuse_test_20M.txt ./temp/;
# end_time=$(date +%s);
# cost_time=$[ $end_time-$start_time ];
# echo "NEW FUse time is $(($cost_time/60))min $(($cost_time%60))s";

# # 读任务
# start_time=$(date +%s);
# ./bin/rtest
# end_time=$(date +%s);
# cost_time=$[ $end_time-$start_time ];
# echo "NEW FUse time is $(($cost_time/60))min $(($cost_time%60))s";

# start_time=$(date +%s);
# cp -rf ../spark ./temp2/;
# end_time=$(date +%s);
# cost_time=$[ $end_time-$start_time ];
# echo "Old Fuse time is $(($cost_time/60))min $(($cost_time%60))s";



# start_time=$(date +%s);
# cp -rf ../spark ./;
# end_time=$(date +%s);
# cost_time=$[ $end_time-$start_time ];
# echo "Ext4 time is $(($cost_time/60))min $(($cost_time%60))s";


# rm -rf ./spark;






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
