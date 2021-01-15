#!/bin/bash
echo "90112" > /proc/sys/vm/min_free_kbytes
echo "100" > /proc/sys/vm/vfs_cache_pressure
echo "20" > /proc/sys/vm/dirty_ratio
echo "60" > /proc/sys/vm/swappiness   
echo "10" > /proc/sys/vm/dirty_background_ratio  
echo "3000" > /proc/sys/vm/dirty_expire_centisecs 
