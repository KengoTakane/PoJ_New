/*============================================================================*/
/*
 * @file    debug.c
 * @brief   デバッグログ出力
 * @note    デバッグログ出力処理を行う。
 * @date    2014/09/01
 */
/*============================================================================*/
/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include "debug.h"

#ifdef __DEBUG__

/*============================================================================*/
/* global */
/*============================================================================*/
static const char *debugHeader[] = {
	"", "FATAL:", "ERROR:", "WARN:", "INFO:", "DEBUG:"
};
//static long	g_dlevel = -1;
//static struct timeval	g_tv;
/*============================================================================*/
/* prototype */
/*============================================================================*/
/*============================================================================*/
/* const */
/*============================================================================*/

/*============================================================================*/
/*
 * @brief   デバッグログ出力処理
 * @note    デバッグログを出力する
 * @param   引数  : dl		ログレベル
 * @param   引数  : fmt		フォーマット文字列
 * @return  戻り値: 出力文字数
 * @date    2014/09/01 [0.0.1] 新規作成
 * 			2024/01/17 [0.0.2] シスログに，ログ種別&ファイル名&行番号，を出力
 */
/*============================================================================*/
int _dprintf(enum dlevel dl, const char *file, int line, const char *fmt, ...)
{
	va_list	varg;
	int	ret = 0;
	//int	base = 10;
	//char	*strdlevel;
	//char	*endptr;
	//struct timeval	tv;
	//time_t	timer;
	//struct tm 	s_tm;
	
#if 0
	if (g_dlevel < 0) {
		// 環境変数DLEVELを取得する
		strdlevel = getenv(DLEVEL);

		// 環境変数DLEVELが定義されていれば
		if (strdlevel != NULL) {
			g_dlevel = strtol(strdlevel, &endptr, base);
			gettimeofday(&g_tv, NULL);
		} else {
			g_dlevel = 3;
		}
	}

	// 環境変数DLEVELが定義されていれば
	if (g_dlevel > 0) {
		// 定義されたレベル以上のログレベルなら
		if ((long)dl <= g_dlevel) {
			fputs(debugHeader[dl], stderr);
			fprintf(stderr, "%s(%d):", file, line);
			time(&timer);
			localtime_r(&timer, &s_tm);
			fprintf(stderr, "%02d%02d%02d%02d%02d:", 
            	s_tm.tm_mon + 1, s_tm.tm_mday,
            	s_tm.tm_hour, s_tm.tm_min, s_tm.tm_sec);
			gettimeofday(&tv, NULL);
//			fprintf(stderr, "%5.5f:", (tv.tv_sec + tv.tv_usec * 1e-6) - (g_tv.tv_sec + g_tv.tv_usec * 1e-6));
			fprintf(stderr, "%5.5f:", tv.tv_sec + tv.tv_usec * 1e-6);
			g_tv = tv;

			// デバッグ出力
			va_start(varg, fmt);
			ret = vfprintf(stderr, fmt, varg);
			va_end(varg);
		} else {
			// NOP
		}
	}
#else
	int	prio;
	char LogHeader[128];
	sprintf(LogHeader, "%s%s(%d):", debugHeader[dl], file, line);
	va_start(varg, fmt);
	strcat(LogHeader, fmt);

	switch(dl) {
	case DEBUG:
		prio = LOG_DEBUG;
		break;
	case INFO:
		prio = LOG_INFO;
		break;
	case WARN:
		prio = LOG_WARNING;
		break;
	case ERROR:
		prio = LOG_ERR;
		break;
	case FATAL:
		prio = LOG_CRIT;
		break;
	}
	openlog("hjpf", LOG_PID|LOG_NDELAY, LOG_DAEMON);
	//vsyslog(prio, fmt, varg);
	//va_start(varg, LogHeader);
	vsyslog(prio, LogHeader, varg);
	va_end(varg);
	closelog();

#endif

	return ret;
}

#endif // __DEBUG__
