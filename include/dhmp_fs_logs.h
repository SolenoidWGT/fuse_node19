#ifndef DHMP_FS_LOGS_H
#define DHMP_FS_LOGS_H

enum fuse_dhmp_log_level{
	FUSE_DHMP_LOG_LEVEL_ERROR,
	FUSE_DHMP_LOG_LEVEL_WARN,
	FUSE_DHMP_LOG_LEVEL_INFO,
	FUSE_DHMP_LOG_LEVEL_DEBUG,
	FUSE_DHMP_LOG_LEVEL_TRACE,
	FUSE_DHMP_LOG_LEVEL_LAST
};

extern enum fuse_dhmp_log_level fuse_global_log_level;

void FUSE_dhmp_log_impl(const char *file, unsigned line, const char *func,
					unsigned log_level, const char *fmt, ...);

#define FUSE_dhmp_log(level, fmt, ...)\
	do{\
		if(level < FUSE_DHMP_LOG_LEVEL_LAST && level<=fuse_global_log_level)\
			FUSE_dhmp_log_impl(__FILE__, __LINE__, __func__,\
							level, fmt, ## __VA_ARGS__);\
	}while(0)

#define	FUSE_ERROR_LOG(fmt, ...)		FUSE_dhmp_log(FUSE_DHMP_LOG_LEVEL_ERROR, fmt, \
									## __VA_ARGS__)
#define	FUSE_WARN_LOG(fmt, ...) 		FUSE_dhmp_log(FUSE_DHMP_LOG_LEVEL_WARN, fmt, \
									## __VA_ARGS__)
#define FUSE_INFO_LOG(fmt, ...)			FUSE_dhmp_log(FUSE_DHMP_LOG_LEVEL_INFO, fmt, \
									## __VA_ARGS__)	
#define FUSE_DEBUG_LOG(fmt, ...)		FUSE_dhmp_log(FUSE_DHMP_LOG_LEVEL_DEBUG, fmt, \
									## __VA_ARGS__)
#define FUSE_TRACE_LOG(fmt, ...)		FUSE_dhmp_log(FUSE_DHMP_LOG_LEVEL_TRACE, fmt,\
									## __VA_ARGS__)


// #ifdef DEBUG_ON
// 	#define DEBUG(x) {sprintf(msg," %s ",x); fwrite(msg, strlen(msg), 1, fp);	fflush(fp);}
// 	#define DEBUG_INT(x) {sprintf(msg_tmp," %d\t",(int)x);DEBUG(msg_tmp);}
// 	#define DEBUG_POINTER(x) {sprintf(msg_tmp," %p\t", (void*)x);DEBUG(msg_tmp);}
// 	#define DEBUG_END() {sprintf(msg,"\n"); fwrite(msg, strlen(msg), 1, fp);	fflush(fp);}
// 	#define DEBUG_P(x)  {sprintf(msg_tmp," %x\t",x);DEBUG(msg_tmp);}
// #else
// 	#define DEBUG(x) {}
// 	#define DEBUG_INT(x) {}
// 	#define DEBUG_END() {}
// 	#define DEBUG_P(x) {}
// #endif




#endif
