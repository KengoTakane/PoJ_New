/*============================================================================*/
/*
 * @file    process.h
 * @brief   process management
 * @note    process management
 * @date    2023/11/13
 */
/*============================================================================*/
#ifndef __PROCESS_H
#define __PROCESS_H

/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>

/*============================================================================*/
/* define */
/*============================================================================*/
#define DEF_PROC_TEST (0)					//テスト用
#define DEF_CMD_MAX (256)					//コマンドの最大文字数
#define DEF_PATH_MAX (128)					//パスの最大文字数
#define DEF_ARG_MAX (32)					//引数の最大数
#define DEF_ARG_STR_MAX (128)				//引数の最大文字数
#define DEF_PROC_MAX (128)					//プロセスの最大数
#define DEF_FAILED_FORK (-1)				//fork失敗時に_processInfoのメンバpidに格納する値
#define DEF_PROC_ALIVE (1)					//プロセス活動時に共有メモリに書き込む値
#define DEF_PROC_DEAD (0)					//プロセス不活時に共有メモリに書き込む値
#define DEF_MONIT_CYCLE (10)				//プロセス監視周期
#define DEF_KILL_WAIT_CYCLE (100)			//プロセス終了待ち周期
#define DEF_KILL_WAIT_NUM (10)				//プロセス終了待ち回数
#define DEF_CPU_MIN (-1)					//設定パラメータcpuの最小値
#define DEF_CPU_MAX (11)					//設定パラメータcpuの最大値
#define DEF_PRIO_MIN (0)					//設定パラメータprioの最小値
#define DEF_PRIO_MAX (99)					//設定パラメータprioの最大値
#define DEF_PERIOD_MIN (0)					//設定パラメータperiodの最小値
#define DEF_RESTART_OFF (0)					//プロセスが再起動がOFF
#define DEF_RESTART_ON (1)					//プロセス再起動がON
#define DEF_CPURATE_MIN (0)					//設定パラメータcpu_rateの最小値
#define DEF_MEMRATE_MIN (0)					//設定パラメータmem_rateの最小値
#define DEF_RESTART_NONE (0)				//プロセス再起動回数が0
#define DEF_RESTART_WARN (1)				//プロセス再起動回数が1
#define DEF_RESTART_FAIL (3)				//プロセス再起動回数が3(現在故障)
#define DEF_CANCEL_RESTART_TIME (1000)		//プロセス再起動回数をリセットする経過時間のしきい値
#define DEF_PROC_SHMMNG_NAME "/procstat"	//プロセス管理の共有メモリ名
#define DEF_TIMER_KIND_PROCESS (1)			//タイマID
#define DEF_MAX_CPU_AND_MEM (3)				//CPUとメモリの使用上限をN回連続で超過した場合再起動

/*============================================================================*/
/* typedef */
/*============================================================================*/
typedef struct _processInfo
{
	char cmd[DEF_CMD_MAX];
	char top_cmd[DEF_CMD_MAX];
	int cpu;
	int prio;
	int period;
	int pid;
	int restart;
	int cpu_rate;
	int mem_rate;
	int cnt_cpu;
	int cnt_mem;
} processInfo;

typedef struct _procStat		/* プロセスの状態 */
{
	int num;
	int stat[DEF_PROC_MAX];		/* 死活情報 */
	float cpu[DEF_PROC_MAX];	/* CPU使用率 */
	float mem[DEF_PROC_MAX];	/* Memory使用率 */
} procStat;

typedef struct _processReStart
{
	int num;
	int time;
} processReStart;

/*============================================================================*/
/* enum */
/*============================================================================*/

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
extern void* ProcMonit(void *arg);

/*============================================================================*/
/* Macro */
/*============================================================================*/

#endif	/* __PROCESS_H */
