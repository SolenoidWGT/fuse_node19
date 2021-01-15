# 查找目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_SRCS)
list(REMOVE_ITEM DIR_SRCS ./test_list.c)   # 注意列表操作中，你的列表变量不加花括号

#set (CMAKE_CXX_FLAGS "-Wall -lxml2 -lrdmacm -libverbs -lpthread -lnuma -lrt -lm")
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-msse2") 
set (EXECUTABLE_OUTPUT_PATH ${FUSE_SOURCE_DIR}/bin)

# 指定生成目标
add_executable(fuse ${DIR_SRCS})
add_executable(test_list ./test_list.c)
# add_executable(hash dhmp_fs_hashmap.c)

add_subdirectory(test)

# 注意编译之前要export环境变量
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
  pkg_check_modules(GTK "fuse3")
  if (GTK_FOUND)
    target_link_libraries(fuse ${GTK_LIBRARIES})
    add_definitions(${GTK_CFLAGS} ${GTK_CFLAGS_OTHER})

    target_link_libraries(fuse_naive ${GTK_LIBRARIES})
    add_definitions(${GTK_CFLAGS} ${GTK_CFLAGS_OTHER})
  endif()
endif()

# 添加链接库
target_link_libraries(fuse ${FUSE_SOURCE_DIR}/dhmp_client/lib/libdhmp.a)
target_link_libraries(fuse xml2)
target_link_libraries(fuse pthread)
target_link_libraries(fuse rdmacm)
target_link_libraries(fuse ibverbs)
target_link_libraries(fuse numa)
target_link_libraries(fuse rt)
target_link_libraries(fuse m)
target_link_libraries(fuse json-c)




# add_executable(hash fuse_consistent_hash.c )  # ../dhmp_client/src/dhmp_hash.c  ../dhmp_client/src/murmur3_hash.c
# 添加链接库
# target_link_libraries(hash /home/gtwang/FUSE/dhmp_client/lib/libdhmp.a)
# target_link_libraries(hash xml2)
# target_link_libraries(hash pthread)
# target_link_libraries(hash rdmacm)
# target_link_libraries(hash ibverbs)
# target_link_libraries(hash numa)
# target_link_libraries(hash rt)
# target_link_libraries(hash m)
# target_link_libraries(hash json-c)