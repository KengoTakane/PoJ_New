/*============================================================================*/
/*
 * @file    com_timer.c
 * @brief   共通コンポーネント
 * @note    共通処理を行う。
 * @date    2019/12/20
 */
/*============================================================================*/
/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <libgen.h>
#include <pthread.h>
#include "com_shmem.h"
#include "com_timer.h"
#include "debug.h"

/*============================================================================*/
/* global */
/*============================================================================*/
extern pthread_mutex_t g_mutex;      
typedef struct _comTimer {
	struct timespec	now;
	struct timespec	next;
	int	period;
} comTimer;
static comTimer	g_comTimer[DEF_COMM_TIMER_MAX];

/*============================================================================*/
/* prototype */
/*============================================================================*/

/*============================================================================*/
/* const */
/*============================================================================*/
#define DEF_1MICROSECOND 1000
#define DEF_1MILLISECOND 1000000LL
#define DEF_1SECOND 1000000000LL
#define DEF_SYNCHRODATA	"/synchrodata"

/*============================================================================*/
/*
 * @brief   スリープ処理
 * @note    ミリ秒の定周期スリープ
 *                  
 * @return  戻り値: int
 * @date    2019/12/20 [0.0.1] 新規作成
 */
/*============================================================================*/
int com_mtimer(const int id)
{
	struct timespec now;
	int	period;
	int	cnt = 0;

	if (id < 0 || DEF_COMM_TIMER_MAX <= id) {
		dprintf(WARN, "com_mtimer(%d) error\n", id);
		return -1;
	}

	if (g_comTimer[id].period == 0) {
		// dprintf(WARN, "com_mtimer(%d) period=%d error\n", id, g_comTimer[id].period);

		// 起床時刻までスリープ
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &g_comTimer[id].next, NULL);
		g_comTimer[id].now = g_comTimer[id].next;
		return -1;
	}

	// 次回起床時刻を計算
	clock_gettime(CLOCK_MONOTONIC, &now);

	g_comTimer[id].next = g_comTimer[id].now;
	
	// 次回起床時刻が現在時刻以降になるまで
	while (!(g_comTimer[id].next.tv_sec == now.tv_sec && g_comTimer[id].next.tv_nsec > now.tv_nsec) &&
		   !(g_comTimer[id].next.tv_sec > now.tv_sec)) {
		period = g_comTimer[id].period;
		while (period > DEF_1SECOND) {
			g_comTimer[id].next.tv_sec += DEF_1SECOND;
			period -= DEF_1SECOND;
		}
		g_comTimer[id].next.tv_nsec += period * DEF_1MILLISECOND;
		while (g_comTimer[id].next.tv_nsec >= DEF_1SECOND) {
			g_comTimer[id].next.tv_nsec -= DEF_1SECOND;
			g_comTimer[id].next.tv_sec++;
		}
		cnt++;
	}

	// 起床時刻までスリープ
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &g_comTimer[id].next, NULL);
	g_comTimer[id].now = g_comTimer[id].next;

	return cnt;
}

int com_timer_init(const int id, int period)
{
	struct timespec now;
	struct timespec	comStdTimer;
	int32_t	ret;

	if (id < 0 || DEF_COMM_TIMER_MAX <= id) {
		dprintf(WARN, "com_timer_init(%d) error\n", id);
		return -1;
	}

	// 現在時刻を取得
	clock_gettime(CLOCK_MONOTONIC, &now);

	// 共有メモリから基準時刻を取得
	int shmid = com_shmem_open(DEF_SYNCHRODATA, SHM_KIND_PLATFORM);
	if (shmid != DEF_COM_SHMEM_FALSE) {
		ret = com_shmem_read(shmid, &comStdTimer, sizeof(comStdTimer));
		if (ret == DEF_COM_SHMEM_FALSE) {
			dprintf(WARN, "id=%d com_shmem_read(%s) error\n", id, DEF_SYNCHRODATA);
		}
		com_shmem_close(shmid);
	} else {
		dprintf(WARN, "id=%d com_shmem_open(%s) error\n", id, DEF_SYNCHRODATA);
	}

	// 基準時刻が未設定なら
	if (comStdTimer.tv_sec == 0 && comStdTimer.tv_nsec == 0) {
		// 基準時刻をセット
		comStdTimer = now;
		int shmid = com_shmem_open(DEF_SYNCHRODATA, SHM_KIND_PLATFORM);
		com_shmem_write(shmid, &comStdTimer, sizeof(comStdTimer));
		com_shmem_close(shmid);
	}

	// 次回起床時刻を計算
	g_comTimer[id].period = period;
	g_comTimer[id].now = comStdTimer;
	while (period > DEF_1SECOND) {
		g_comTimer[id].now.tv_sec += DEF_1SECOND;
		period -= DEF_1SECOND;
	}
	g_comTimer[id].now.tv_nsec += period * DEF_1MILLISECOND;
	while (g_comTimer[id].now.tv_nsec >= DEF_1SECOND) {
		g_comTimer[id].now.tv_nsec -= DEF_1SECOND;
		g_comTimer[id].now.tv_sec++;
	}
	
	return 0;
}

