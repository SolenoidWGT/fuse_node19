set (EXECUTABLE_OUTPUT_PATH ${FUSE_SOURCE_DIR}/bin)
add_executable(wtest test_write.c)
add_executable(rtest test_read.c)
add_executable(fuse_naive fuse_back.c)



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
target_link_libraries(fuse_naive xml2)
target_link_libraries(fuse_naive pthread)
target_link_libraries(fuse_naive rdmacm)
target_link_libraries(fuse_naive ibverbs)
target_link_libraries(fuse_naive numa)
target_link_libraries(fuse_naive rt)
target_link_libraries(fuse_naive m)
target_link_libraries(fuse_naive json-c)