# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.6)
set(CMAKE_VERBOSE_MAKEFILE ON)
# 项目信息
project (DHMP)

SET(CMAKE_BUILD_TYPE "Debug")  
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")  
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")  

# 添加 math 子目录
include_directories(include)
include_directories(/usr/local/include/libxml2/)
include_directories(/home/rdma_match/xinzh/include)
link_directories(/home/rdma_match/xinzh/lib)

add_subdirectory(src)

add_subdirectory(test)
