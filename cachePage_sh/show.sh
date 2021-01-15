#!/bin/bash
echo "min_free_kbytes:" &&  cat /proc/sys/vm/min_free_kbytes
echo "vfs_cache_pressure:" &&  cat /proc/sys/vm/vfs_cache_pressure
echo "dirty_ratio": &&  cat /proc/sys/vm/dirty_ratio
echo "dirty_bytes": &&  cat /proc/sys/vm/dirty_bytes
echo "swappiness:" &&  cat /proc/sys/vm/swappiness   
echo "dirty_background_ratio:" &&  cat /proc/sys/vm/dirty_background_ratio  
echo "dirty_expire_centisecs:" &&  cat /proc/sys/vm/dirty_expire_centisecs 
