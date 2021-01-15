#!/bin/bash
: << !
echo "63963136" > /proc/sys/vm/min_free_kbytes
echo "50" > /proc/sys/vm/vfs_cache_pressure
echo "1" > /proc/sys/vm/dirty_ratio
echo "100" > /proc/sys/vm/swappiness   
echo "90" > /proc/sys/vm/dirty_background_ratio  
echo "9000" > /proc/sys/vm/dirty_expire_centisecs 
!
echo "12428" > /proc/sys/vm/dirty_bytes
