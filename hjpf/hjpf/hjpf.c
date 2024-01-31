/*============================================================================*/
/*
 * @file    sample.c
 * @brief   
 * @note    
 * @date    2020/01/07
 */
/*============================================================================*/
/*============================================================================*/
/* include */
/*============================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"
#include "process.h"
#include "resource.h"
#include "failsafe.h"
#include "com_fs.h"

/*============================================================================*/
/* global */
/*============================================================================*/
int	gComm_StopFlg = DEF_COMM_OFF;				/* プログラム終了フラグ */   
extern int	errno;
pthread_mutex_t g_mutex;   

/*============================================================================*/
/* const */
/*============================================================================*/
static void AplSigHandler(const int signo);
static void AplInitSignal(void);

threadInfo g_threadInfo[] = {
	{ ProcMonit, "process.conf", -1 },
	{ ResMain, "resource.conf", -1 },
	{ FailsafeMain, "failsafe.conf", -1 },
};

/*============================================================================*/
/*
 * @brief   シグナル処理
 * @note    シグナル処理
 * @param   引数  : signo	シグナル番号
 * @return  戻り値: なし
 * @date    2020/01/07 [0.0.1] 新規作成
 */
/*============================================================================*/
static void AplSigHandler(const int signo)
{
dprintf(INFO, "AplSigHandler=%d\n", signo);
	// 終了フラグをONにする
	gComm_StopFlg = DEF_COMM_ON;
}

/*============================================================================*/
/*
 * @brief   シグナル初期化
 * @note    シグナル初期化
 * @param   引数  : なし
 * @return  戻り値: なし
 * @date    2020/01/07 [0.0.1] 新規作成
 */
/*============================================================================*/
static void AplInitSignal(void)
{
	struct sigaction	sa_sig;
	struct sigaction	sa_sigign;

dprintf(INFO, "AplInitSignal=%d\n");
	// シグナルアクション(SIGTERM,SIGINT)の設定
	memset(&sa_sig, 0, sizeof(sa_sig));
	sa_sig.sa_handler = AplSigHandler;
	sa_sig.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa_sig, NULL);
	sigaction(SIGTERM, &sa_sig, NULL);

	// シグナルアクション(SIGPIPE)の設定
	memset(&sa_sigign, 0, sizeof(sa_sigign));
	sa_sigign.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa_sigign, NULL);
}

/*============================================================================*/
/*
 * @brief   Main処理
 * @note    Main処理
 * @param   引数  : argc
 *
 *
 * @return  戻り値: int
 * @date    2020/01/07 [0.0.1] 新規作成
 */
/*============================================================================*/
int main(int argc, char *argv[])
{
	int ret;
#if 0
	int Priority;
	struct sched_param prio;
	cpu_set_t cpu_set;

	memset(&prio, 0x00, sizeof(prio));

	//root権限設定
	setuid(0);

	//CPUコアを１に固定
	CPU_ZERO(&cpu_set);
	CPU_SET(1, &cpu_set);
	ret = sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set);
	if (ret != 0) {
	    perror("setaffinity");
	    exit(1);
	}

	Priority = sched_get_priority_max(SCHED_FIFO)-40; //優先度を3番目に設定
	prio.sched_priority = Priority;

	if(sched_setscheduler(0,SCHED_FIFO,&prio)<0){
		perror("Schedule set error\n");
	}
#endif
	gComm_StopFlg = DEF_COMM_OFF;

	if (pthread_mutex_init(&g_mutex, NULL) != 0) {                                    
		dprintf(WARN, "pthread_mutex_init() error=%d\n", errno);
		return DEF_COM_SHMEM_FALSE;
	} 

	// シグナル初期化
	AplInitSignal();

	/* 設定ファイルの読み込み */
	com_shmem_conf("memory.conf");

	/* 共有メモリ生成 */
	com_shmem_init();

	//周期タイマ初期化
	com_timer_init(ENUM_TIMER_MAIN, 500);

	// スレッド生成
	for (int cnt = 0; cnt < sizeof(g_threadInfo) / sizeof(threadInfo); cnt++) {
		ret = pthread_create(&g_threadInfo[cnt].threadid, NULL, g_threadInfo[cnt].thread, g_threadInfo[cnt].arg);
		if (ret != DEF_RET_OK) {
			dprintf(WARN, "pthread_create(%d) error=%d\n", cnt, errno);
			return DEF_RET_NG;
		}
	}

	// 終了待ち
	while (gComm_StopFlg == DEF_COMM_OFF) {
		com_mtimer(ENUM_TIMER_MAIN);
		
	}
	// スレッド終了
	for (int cnt = 0; cnt < sizeof(g_threadInfo) / sizeof(threadInfo); cnt++) {
		void *pret;
		ret = pthread_join(g_threadInfo[cnt].threadid, &pret);
		if (ret != DEF_RET_OK) {
			dprintf(WARN, "pthread_join error=%d\n", errno);
		}
	}
	
	/* 共有メモリ削除 */
	com_shmem_destroy();

	dprintf(INFO, "Trans End\n");

	return 0;
}

