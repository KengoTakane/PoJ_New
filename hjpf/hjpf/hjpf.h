/*==============================================*/
/*
 * define
 * @history    2023/11/14  Ver.0.1
 */
/*==============================================*/

#ifndef __HJPF_H
#define __HJPF_H

/*==============================================*/
/* include */
/*==============================================*/
#include <stdint.h>

/*============================================================================*/
/* define */
/*============================================================================*/
/* 共通定義値 */
#define DEF_COMM_ON				(1)
#define DEF_COMM_OFF			(0)
#define DEF_RET_OK				(0)
#define DEF_RET_NG				(-1)

/*==============================================*/
/* typedef */
/*==============================================*/
typedef struct _threadInfo {
	void		*(*thread) (void *arg);
	void		*arg;
	pthread_t	threadid;
} threadInfo;

typedef enum {
	ENUM_TIMER_MAIN = 0,
	ENUM_TIMER_PROC = 1,
	ENUM_TIMER_RES = 2,
	ENUM_TIMER_CAMERA = 3,
	ENUM_TIMER_FAILSAFE = 4,
	ENUM_TIMER_I2C = 5,
	ENUM_TIMER_MAVM = 6,
	ENUM_TIMER_MAVW = 7,
	ENUM_TIMER_PING = 8,
} ENUM_TIMER_ID;

#endif /*__HJPF_H */
