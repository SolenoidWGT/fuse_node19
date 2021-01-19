#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../include/dhmp_fs_logs.h"
//#define DEBUG
//#define WGT

#define FUSE_LOG_TIME_FMT "%04d/%02d/%02d-%02d:%02d:%02d.%05ld"
enum fuse_dhmp_log_level fuse_global_log_level=FUSE_DHMP_LOG_LEVEL_DEBUG;

const char *const fuse_level_str[]=
{
	"ERROR", "WARN", "INFO", "DEBUG","TRACE" 
};

void FUSE_dhmp_log_impl(const char * file, unsigned line, const char * func, unsigned log_level, const char * fmt, ...)
{
	
#ifdef DEBUG
	va_list args;
	char mbuf[2048];
	
	int mlength=0;

	const char *short_filename;
	char filebuf[270];//filename at most 256

	struct timeval tv_now;
	struct tm tm_now;
	time_t tt_now;


	/*get the args,fill in the string*/
	va_start(args,fmt);
	mlength=vsnprintf(mbuf,sizeof(mbuf),fmt,args);
	va_end(args);
	mbuf[mlength]=0;

	/*get short file name*/
	short_filename=strrchr(file,'/');
	short_filename=(!short_filename)?file:short_filename+1;


	snprintf(filebuf,sizeof(filebuf),"%s:%u",short_filename,line);

	/*get the time*/
	gettimeofday(&tv_now,NULL);
	tt_now=(time_t)tv_now.tv_sec;
	localtime_r(&tt_now,&tm_now);




#ifdef WGT
FILE * fuse_log_fp = fopen("/home/gtwang/FUSE/gtwang_fuse_logs.txt", "a+");
#else
FILE * fuse_log_fp = fopen("/home/gtwang/FUSE/fuse_logs.txt", "a+");
#endif
	
	fprintf(fuse_log_fp,
		"[" FUSE_LOG_TIME_FMT "] %-28s [%-5s] - %s\n",
		tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
		tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, tv_now.tv_usec,
		filebuf,
		fuse_level_str[log_level], mbuf);
	fflush(fuse_log_fp);
	fclose(fuse_log_fp);
#endif
}



void FUSE_journal_impl(FILE *file,  const char * fmt, ...)
{
	va_list args;
	char mbuf[2048];
	int mlength=0;
#ifdef DEBUG
	/*get the args,fill in the string*/
	va_start(args,fmt);
	mlength=vsnprintf(mbuf,sizeof(mbuf),fmt,args);
	va_end(args);
	mbuf[mlength]=0;

	//FILE * fuse_journa_fp = fopen("/home/gtwang/FUSE/fuse_journal.txt", "a+");
	fprintf(file, "%s\n", mbuf);
	fflush(file);
	//fclose(fuse_journa_fp);
#endif
}
