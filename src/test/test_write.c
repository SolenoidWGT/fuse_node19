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
int n;
int fp;
#define BANK_SIZE (1024*1024*20)          // BANK大小，20M
/*
    warning: implicit declaration of function ‘clock_gettime’ [-Wimplicit-function-declaration]
         clock_gettime(CLOCK_MONOTONIC, &start_test);
    这种警告的解决方法：
    如果看到编译的时候提示wall,我们首先是找到报警搞的函数，再用man 命令来 man 函数，然后找到头文件就，加上即可。
*/


int main(int argc, char **argv) {
    struct timespec start_test, end_test;
    unsigned long long total_write_time = 0, total_fsync_time = 0; 
    char * test_file = (char *)malloc( BANK_SIZE);
    int i=0, chunk_sum =0;
    memset(test_file, 0, BANK_SIZE);
    for(i=0; i<BANK_SIZE; i++){
        test_file[i] = (rand() % 10 + '0');
        // test_file[i] = '1';
        chunk_sum += (test_file[i] - '0');
    }
    printf("chunk_sum is %d \n!", chunk_sum);
    


    if((fp= open("/home/gtwang/FUSE/fuse_test_20M.txt", O_APPEND| O_RDWR|O_CREAT, S_IRWXU)) ==-1 ){
        printf("Open file fail! %s\n", strerror(errno));
    }

    if( (n=write(fp, test_file , BANK_SIZE)) == -1){
        printf("write file fail %s!\n", strerror(errno));
    }



    if( (n=read(fp, test_file , BANK_SIZE)) == -1){
        printf("write file fail %s!\n", strerror(errno));
    }


    // int chunk_sum_2 = 0;
    // for(i=0; i<BANK_SIZE; i++){
    //     chunk_sum_2 += (test_file[i] - '0');
    // }
    // if(chunk_sum_2 == chunk_sum){
    //     printf("test sucess!\n");
    // }else{
    //     printf("test fail!\n");
    // }

    if( (n = close(fp)) == -1){
        printf("CLose file ERROR!, %s\n ", strerror(errno));
    }


    return 0;
}