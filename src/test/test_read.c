#include <sys/types.h>//这里提供类型pid_t和size_t的定义
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#define CLOCK_MONOTONIC 1
#define TEST_TYPE  6
#define TEST_NUM  10000
char str_64[64];
char str_256[256];
char str_2k[1024*2];
char str_8k[1024*8];
char str_32k[1024*32];
char str_128k[1024*128];
// C语言中，当使用变量来定义数组长度的时候，不能同时进行初始化赋值, 所以得使用宏
char * str[TEST_TYPE] = {str_64, str_256, str_2k, str_8k, str_32k, str_128k};
int size_array[TEST_TYPE] = {64, 256, 1024*2, 1024*8, 1024*32, 1024*128};
int n, re;
int fp;
#define BANK_SIZE (1024*1024*20)          // BANK大小，10M
/*
    warning: implicit declaration of function ‘clock_gettime’ [-Wimplicit-function-declaration]
         clock_gettime(CLOCK_MONOTONIC, &start_test);
    这种警告的解决方法：
    如果看到编译的时候提示wall,我们首先是找到报警搞的函数，再用man 命令来 man 函数，然后找到头文件就，加上即可。
*/


int main(int argc, char **argv) {
    struct timespec start_test, end_test;
    unsigned long long total_write_time = 0, total_fsync_time = 0; 
    char * test_file = (char *)malloc(BANK_SIZE);

    int i=0, chunk_sum =0;

    // BANK_SIZE == 12288，是fuse日志里面读每一次上层应用读的大小
    char * src = (char *)malloc(BANK_SIZE);
    char * dest = (char *)malloc(BANK_SIZE);



    fp= open("/home/gtwang/FUSE/fuse_test_10M.txt", O_APPEND| O_RDWR|O_CREAT, S_IRWXU);
    if(fp ==-1 ){
        printf("Open file fail! %s\n", strerror(errno));
    }

    // 测试read时间
    clock_gettime(CLOCK_MONOTONIC, &start_test);
    for(i=0; i<52; i++){
        re =  read(fp, test_file , BANK_SIZE);
        if(re == -1){
            printf("write file fail %s!\n", strerror(errno));
            free(test_file);
            break;
        }
         n += re;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_test);
    total_write_time = ((end_test.tv_sec * 1000000000) + end_test.tv_nsec) -
            ((start_test.tv_sec * 1000000000) + start_test.tv_nsec); 

    printf("Total read time is %lf ms, read size is %d \n", (double)(total_write_time/1000000), n);

    // // 测试内存拷贝时间
    // clock_gettime(CLOCK_MONOTONIC, &start_test);
    // memcpy(dest, test_file, BANK_SIZE);
    // clock_gettime(CLOCK_MONOTONIC, &end_test);
    // total_write_time = ((end_test.tv_sec * 1000000000) + end_test.tv_nsec) -
    //         ((start_test.tv_sec * 1000000000) + start_test.tv_nsec); 
    // printf("memcpy time is %lf us\n",  (double)(total_write_time/1000));

    if( (n = close(fp)) == -1){
        printf("CLose file ERROR!, %s\n ", strerror(errno));
    }


    return 0;
}