/*============================================================================*/
/*
 * @file    resource.h
 * @brief   resource management
 * @note    resource management
 * @date    2023/11/27
 */
/*============================================================================*/
#ifndef __RESOURCE_H
#define __RESOURCE_H

/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>

/*============================================================================*/
/* define */
/*============================================================================*/
#define DEF_RES_TEST (0)					//リソーステスト用
#define DEF_STR_MAX (256)					//文字最大数
#define DEF_CPU_NUM (12)					//CPU数
#define DEF_DECIMAL (10)					//数値変換時の基数
#define DEF_PERIOD_MIN (0)					//収集周期の最小(監視しない)
#define DEF_MONIT_CYCLE (10)				//監視周期
#define DEF_RES_SHMMNG_NAME "/resstat"		//リソース共有メモリ名
#define DEF_TIMER_KIND_RESOURCE (2)			//タイマID
#define DEF_PING_MAX (10)

/*============================================================================*/
/* enum */
/*============================================================================*/
enum res_kind {
	RES_KIND_CPU_LOAD = 0,
	RES_KIND_MEM_LOAD = 1,	
	RES_KIND_DISK_LOAD = 2,
	RES_KIND_CPU_THERM = 3,
	RES_KIND_MAX
};

enum meminfo_kind{
	MEMINFO_KIND_TOTAL = 0,
	MEMINFO_KIND_FREE = 1,
	MEMINFO_KIND_AVAIL = 2,
	MEMINFO_KIND_BUF = 3,
	MEMINFO_KIND_CASHE = 4,
	MEMINFO_KIND_MAX
};

/*============================================================================*/
/* typedef */
/*============================================================================*/
typedef struct _resourceInfo{			/* リソース監視周期（リソース種別ごと） */
	int period[RES_KIND_MAX];
} resourceInfo;

typedef struct _resourceStat{
	int cpu_load[DEF_CPU_NUM + 1];		/* CPU負荷 */
	int mem_load;						/* メモリ使用 */
	int disk_load;						/* ディスク使用量[%] */
	int cpu_therm;						/* CPU温度[1/1000℃] */
} resourceStat;

typedef struct _linuxProcStat{
	char name[DEF_STR_MAX];
	int user;
	int nice;
	int system;
	int idle;
	int iowait;
	int irq;
	int softirq;
	int steal;
	int guest;
	int guest_nice;
} linuxProcStat;

typedef struct _confSectionTbl{
	int index;
	char *item;
} confSectionTbl;

typedef struct _resourceUARTInfo {
	char devname[128];
	int timeout;
} resUARTInfo;

typedef struct _res2CInfo {
	char devname[128];
	int	period;
	int timeout;
} resI2CInfo;

typedef struct _resPingInfo {
	int	pingNum;
	char shmname[DEF_PING_MAX][128];
	char hostname[DEF_PING_MAX][128];
	int	period;
} resPingInfo;

typedef struct _resMavlinkInfo {
	char devname[128];
	int baudrate;
	int read_period;
	int write_period;
	int timeout;
} resMavlinkInfo;

typedef struct _resCameraInfo {
	char devname[128];
	int cameranum;
	int period;
	int timeout;
} resCameraInfo;

typedef struct _resThreadInfo {
	void		*(*thread) (void *arg);
	void		*arg;
	pthread_t	threadid;
	int			*isStart;
} resThreadInfo;

/*============================================================================*/
/* struct */
/*============================================================================*/

/*============================================================================*/
/* func */
/*============================================================================*/

/*============================================================================*/
/* extern(val) */
/*============================================================================*/

/*============================================================================*/
/* extern(func) */
/*============================================================================*/
extern void* ResMain(void* arg);
extern int com_serial_open(char *devname, int BaudRate);
extern void com_serial_close(int fd);
extern char *strtoks(char *s1, const char *s2);

/*============================================================================*/
/* Macro */
/*============================================================================*/

#endif	/* __RESOURCE_H */
