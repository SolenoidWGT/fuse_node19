#!/usr/bin/python
# -*- coding: UTF-8 -*-

# 打开一个文件
import sys, time
try:
    i = 0
    meta = 128   # 粒度，例如128表示128k
    G_num = 1    # IO大小，单位G
    kb = 1024
    mb = 1024/meta
    G =  1024
    count = mb*G*G_num   # 操作次数
    total = meta*kb*count   # 总量，单位byte
    inputFile = 'out.txt'  # 输入文件
    fo_read = open(inputFile, "r+")
    startT = time.time()
    endT = time.time()
    runtime = (endT-startT)
    while (i < count):
        read = fo_read.read(meta*kb)
        i+=1
        percent = int(float(i*100)/float(count))
        sys.stdout.write('\r%s%%'%(percent))
        endT = time.time()
        runtime = (endT-startT)
        sys.stdout.write('%s%s' %('       RUNTIME: ',(endT - startT)))
        sys.stdout.flush()
    io_sum = total/(1024*1024)
    bandwidth = (format(float(io_sum)/float(runtime),'.2f'))
    avg_ops = (format(float(runtime*1000)/float(count),'.4f'))
    iops = (format(float(count)/float(runtime),'.2f'))
    print
    print('Done!')
    print('--------------------------------------')
    print('%s%f%s' %('io_summary: ',io_sum,' MB'))
    print('%s%s%s' %('bandwidth: ', bandwidth ,' MB/s'))
    print('%s%s%s' %('avg_ops: ',avg_ops,' ms'))
    print('%s%s%s' %('iops: ',iops,' op/s'))
    print('%s%s%s' %('RUNTIME: ',(endT - startT),' SECs'))
    print('--------------------------------------')
    # 关闭打开的文件
    fo_read.close()
except IOError:
    print "Error: file not found or open failed"
    fo_read.close()
else:
    fo_read.close()

