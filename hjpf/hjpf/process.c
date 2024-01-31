/*============================================================================*/
/*
 * @file    process.c
 * @brief   プロセス管理
 * @note    プロセス管理
 * @date    2023/11/13
 */
/*============================================================================*/

/*============================================================================*/
/* include */
/*============================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sched.h>
#include <errno.h>
#include "process.h"
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"

/*============================================================================*/
/* global */
/*============================================================================*/
static processInfo ProcessInfo[DEF_PROC_MAX];
static procStat ProcStat;
#if 0
procStat ProcStat2;
#endif
static int ProcNum = 0;
extern int gComm_StopFlg;

/*============================================================================*/
/* prototype */
/*============================================================================*/
static void ProcCmd(char str[], char pathname[], char* argptr[]);
static int ProcLaunch(int id);
static int ProcReadFile(char filename[]);
static int ProcInit(void);
static int ProcTerm(void);

/*============================================================================*/
/* const */
/*============================================================================*/

/*============================================================================*/
/*
 * @brief   パスと引数の抽出処理
 * @note    process.conf内の[process]->cmdからパスと引数を抽出する
 * @param   引数  : str			command
 * @param   		filename	filename
 * @param   		argptr		argument
 *                  
 * @return  戻り値: void
 * @date    2023/11/13 [0.0.1] 新規作成
 */
/*============================================================================*/
static void ProcCmd(char str[], char pathname[], char* argptr[])
{
	static char argv[DEF_ARG_MAX][DEF_ARG_STR_MAX];
	char	tmpstr[DEF_ARG_STR_MAX];
	char* tp;
	int i = 0;
	
	strcpy(tmpstr, str);

	//パス切り出し
	tp = strtok(tmpstr, " ");
	strcpy(pathname, tp);
	
	strcpy(argv[i], tp);
	argptr[i] = argv[i];
	i++;

	//引数切り出し
	while(tp != NULL)
	{
		tp = strtok(NULL, " ");
		if(tp != NULL)
		{
			strcpy(argv[i], tp);
			argptr[i] = argv[i];
		}
		else
		{
			argptr[i] = NULL;
		}
		i++;
	}
}

/*============================================================================*/
/*
 * @brief   ファイル読み込み処理
 * @note    設定ファイル(process.conf)の読み込み
 * @param   引数  : char filename
 *                  
 * @return  戻り値: int
 * @date    2023/11/13 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ProcReadFile(char filename[])
{
	GKeyFile *file;
	GError *err = NULL;
	gchar** group;
	gsize group_size;
	int ret = 0;
	gchar* cmd_value;

	file = g_key_file_new();

	// ファイルオープン
	if(!g_key_file_load_from_file(file, filename, 0, &err))
	{
dprintf(ERROR, "load file failed. filename = %s. err = %s\n", filename, err->message);
		ret = DEF_RET_NG;
	}
	else
	{
		// グループ取得
		group = g_key_file_get_groups(file, &group_size);
		
		for(int i = 0; i < group_size; i++)
		{
			// コマンド取得
			if(NULL == (cmd_value = g_key_file_get_string(file, group[i], "cmd", &err)))
			{
dprintf(ERROR, "load cmd failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{
				strcpy(ProcessInfo[ProcNum].cmd, (char*)cmd_value);

				char pathname[DEF_PATH_MAX];
				char *argptr[DEF_ARG_MAX];
				struct stat buf;
				ProcCmd(ProcessInfo[ProcNum].cmd, pathname, argptr);
				
				if(stat(pathname, &buf) == -1)
				{
dprintf(ERROR, "path search failed. %s\n", pathname);
					printf("error\n");
					ret = DEF_RET_NG;
				}
			}
			
			// cpu取得
			if(0 == (ProcessInfo[ProcNum].cpu = g_key_file_get_integer(file, group[i], "cpu", &err)) && (NULL != err))
			{
dprintf(ERROR, "load cpu failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{
				if(ProcessInfo[ProcNum].cpu < DEF_CPU_MIN || ProcessInfo[ProcNum].cpu > DEF_CPU_MAX)
				{
dprintf(ERROR, "cpu value failed. %d\n", ProcessInfo[ProcNum].cpu);
					ret = DEF_RET_NG;
				}
			}
			
			// priority取得
			if(0 == (ProcessInfo[ProcNum].prio = g_key_file_get_integer(file, group[i], "prio", &err)) && (NULL != err))
			{
dprintf(ERROR, "load priority failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{
				if(ProcessInfo[ProcNum].prio < DEF_PRIO_MIN || ProcessInfo[ProcNum].prio > DEF_PRIO_MAX)
				{
dprintf(ERROR, "priority value failed. %d\n", ProcessInfo[ProcNum].prio);
					ret = DEF_RET_NG;
				}
			}
			
			// period取得 
			if(0 == (ProcessInfo[ProcNum].period = g_key_file_get_integer(file, group[i], "period", &err)) && (NULL != err))
			{
dprintf(ERROR, "load period failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{ 
				if(ProcessInfo[ProcNum].period < DEF_PERIOD_MIN || ProcessInfo[ProcNum].period % 10 != 0)
				{
dprintf(ERROR, "period value failed. %d\n", ProcessInfo[ProcNum].period);
					ret = DEF_RET_NG;
				}
			}
			
			// restart取得 
			if(0 == (ProcessInfo[ProcNum].restart = g_key_file_get_integer(file, group[i], "restart", &err)) && (NULL != err))
			{
dprintf(ERROR, "load restart failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{ 
				if(DEF_RESTART_OFF > ProcessInfo[ProcNum].restart || ProcessInfo[ProcNum].restart > DEF_RESTART_ON)
				{
dprintf(ERROR, "restart value failed. %d\n", ProcessInfo[ProcNum].restart);
					ret = DEF_RET_NG;
				}
			}

			// CPU使用率取得 
			if(0 == (ProcessInfo[ProcNum].cpu_rate = g_key_file_get_integer(file, group[i], "cpu_rate", &err)) && (NULL != err))
			{
dprintf(ERROR, "load cpu_rate failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{ 
				if(DEF_CPURATE_MIN > ProcessInfo[ProcNum].cpu_rate)
				{
dprintf(ERROR, "restart cpu_rate failed. %d\n", ProcessInfo[ProcNum].cpu_rate);
					ret = DEF_RET_NG;
				}
			}

			// メモリ使用率取得 
			if(0 == (ProcessInfo[ProcNum].mem_rate = g_key_file_get_integer(file, group[i], "mem_rate", &err)) && (NULL != err))
			{
dprintf(ERROR, "load mem_rate failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{ 
				if(DEF_CPURATE_MIN > ProcessInfo[ProcNum].mem_rate)
				{
dprintf(ERROR, "restart cpu_rate failed. %d\n", ProcessInfo[ProcNum].mem_rate);
					ret = DEF_RET_NG;
				}
			}
			
			g_free(cmd_value);
			ProcNum++;
		}
		g_strfreev(group);
	}
	
	g_key_file_free(file);
	if(NULL != err)
	{
		g_error_free(err);
	} 
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   プロセス起動処理
 * @note    プロセス生成、CPUと優先度の割り当て
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/13 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ProcLaunch(int id)
{
	int ret = DEF_RET_OK;
	int ret_sched;
	pid_t pid = fork();
		
	//エラー
	if(pid < 0)
	{
dprintf(ERROR, "fork failed.\n");
		ProcessInfo[id].pid = DEF_FAILED_FORK;
		ret = DEF_RET_NG;
	}
	//子プロセス処理
	else if(pid == 0)
	{
		char pathname[DEF_PATH_MAX];
		char* argptr[DEF_PROC_MAX];
		char* envp[DEF_PROC_MAX];
		ProcCmd(ProcessInfo[id].cmd, pathname, argptr);
		envp[0] = NULL;

#if 0			
for (int cnt=0; argptr[cnt]; cnt++){
dprintf(ERROR, "[%s] [%s] arg[%d]=[%s] \n", ProcessInfo[i].cmd, pathname, cnt, argptr[cnt]);
}
#endif
		execve(pathname, argptr, envp);
	}
	//親プロセス処理
	else
	{
		ProcessInfo[id].pid = pid;
		struct sched_param prio;
		setuid(0);
		
		//cpu割り当て
		if(ProcessInfo[id].cpu >= 0)
		{
			cpu_set_t cpu_set;
			
			CPU_ZERO(&cpu_set);
			CPU_SET(ProcessInfo[id].cpu, &cpu_set);
			if(sched_setaffinity(ProcessInfo[id].pid, sizeof(cpu_set_t), &cpu_set) == -1)
			{
dprintf(ERROR, "cpu set failed. pid = %d\n", ProcessInfo[id].pid);
				ProcTerm();
				ret = DEF_RET_NG;
			}
		}
		
		//優先度割り当て
		prio.sched_priority = ProcessInfo[id].prio;
		if(ProcessInfo[id].prio == 0)
		{
			ret_sched = sched_setscheduler(ProcessInfo[id].pid, SCHED_OTHER, &prio);
		}
		else
		{
			ret_sched = sched_setscheduler(ProcessInfo[id].pid, SCHED_FIFO, &prio);
		}
		
		if(ret_sched < 0)
		{
dprintf(ERROR, "priority set failed. pid = %d, errno = %d\n", ProcessInfo[id].pid, errno);
			ProcTerm();
			ret = DEF_RET_NG;
		}
	}
	
	return ret;
}


/*============================================================================*/
/*
 * @brief   プロセス初期処理
 * @note    全プロセス起動
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/13 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ProcInit(void)
{
	int	ret = DEF_RET_OK;

	for(int i = 0; i < ProcNum; i++)
	{
		if(ProcLaunch(i) == DEF_RET_NG)
		{
dprintf(ERROR, "ProcInit(%i) failed.\n", i);
			ret = DEF_RET_NG;
		}
	}
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   monit process
 * @note    monitor process and write life or death
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/13 [0.0.1] 新規作成
 */
/*============================================================================*/
void* ProcMonit(void *arg)
{
	processReStart ProcessReStart[DEF_PROC_MAX];
	int time = DEF_RET_OK;
	int status;
	int	ret;
	int id;
	char buf[1024];
	FILE *fp;	
	int pid, pr, ni, virt, res, shr;
	float cpu, mem;
	char user[32], S[5];
	char cmd[256];
	
	com_timer_init(ENUM_TIMER_PROC, DEF_MONIT_CYCLE);
	
	// 設定ファイル読み込み
	ret = ProcReadFile(arg);
	if (ret == DEF_RET_NG) {
dprintf(ERROR, "ProcReadFile(%s) failed.\n", arg);
		pthread_exit(NULL);
	}
	
	// プロセス初期化
	ret = ProcInit();
	if (ret == DEF_RET_NG) {
dprintf(ERROR, "ProcInit() failed.\n");
		pthread_exit(NULL);
	}
	
	//共有メモリオープン
	id = com_shmem_open(DEF_PROC_SHMMNG_NAME, SHM_KIND_PLATFORM);
	if(id == DEF_COM_SHMEM_FALSE){
                dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_PROC_SHMMNG_NAME);
		pthread_exit(NULL);
	}

	ProcStat.num = ProcNum;

	while(gComm_StopFlg == DEF_COMM_OFF)
	{
		for(int i = 0; i < ProcNum; i++)
		{
			if(time % ProcessInfo[i].period == 0 && ProcessInfo[i].period != DEF_PERIOD_MIN && ProcessInfo[i].pid != DEF_FAILED_FORK)
			{
				//共有メモリ書き込み
				if(waitpid(ProcessInfo[i].pid, &status, WNOHANG) == 0)
				{
					// プロセスが活動中
#if DEF_PROC_TEST
					printf("[%d]alive. pid = %d. restart time = %d. cnt = %d.\n", i, ProcessInfo[i].pid, ProcessReStart[i].time, ProcessReStart[i].num);
#endif				
					//プロセスCPU、メモリ使用率計測
					sprintf(cmd, "%s%d", "top -b -n 1 | grep --line-buffered ", ProcessInfo[i].pid);
					fp = popen(cmd, "r");
					if(fp == NULL)
					{
						dprintf(WARN, "top error. cmd = %s\n", cmd);
					}
					fgets(buf, 1024, fp);
					sscanf(buf, "%d %s %d %d %d %d %d %s %f %f", &pid, user, &pr, &ni, &virt, &res, &shr, S, &cpu, &mem);
					pclose(fp);

					ProcStat.cpu[i] = cpu;
					ProcStat.mem[i] = mem;

					if(cpu > (float)ProcessInfo[i].cpu_rate)
					{
						ProcessInfo[i].cnt_cpu++;
						dprintf(WARN, "%d cnt_cpu = %d\n", ProcessInfo[i].pid, ProcessInfo[i].cnt_cpu);
					}
					else
					{
						ProcessInfo[i].cnt_cpu = 0;
					}

					if(mem > (float)ProcessInfo[i].mem_rate)
					{
						ProcessInfo[i].cnt_mem++;
						dprintf(WARN, "%d cnt_mem = %d\n", ProcessInfo[i].pid, ProcessInfo[i].cnt_mem);
					}
					else
					{
						ProcessInfo[i].cnt_mem = 0;
					}

					if(ProcessInfo[i].cnt_cpu > DEF_MAX_CPU_AND_MEM)
					{
						dprintf(ERROR, "Killed %d due to exceeding CPU limit.\n", ProcessInfo[i].pid);
						kill(ProcessInfo[i].pid, SIGINT);
					}
					else if(ProcessInfo[i].cnt_mem > DEF_MAX_CPU_AND_MEM)
					{
						dprintf(ERROR, "Killed %d due to exceeding MEMORY limit.\n", ProcessInfo[i].pid);
						kill(ProcessInfo[i].pid, SIGINT);
					}

					//タイムアウト処理
					if(ProcessReStart[i].time >= DEF_CANCEL_RESTART_TIME)
					{
						ProcessReStart[i].num = 0;
						ProcStat.stat[i] = 0;
					}
					com_shmem_write(id, &ProcStat, sizeof(ProcStat));
				}
				else
				{
					// プロセスが不活
#if DEF_PROC_TEST
					printf("[%d]dead. pid = %d. restart time = %d. cnt = %d.\n", i, ProcessInfo[i].pid, ProcessReStart[i].time, ProcessReStart[i].num);
#endif
					ProcessInfo[i].pid = -1;
					
					//　プロセス再起動
					if(ProcessInfo[i].restart == DEF_RESTART_ON)
					{
						ProcessReStart[i].num++;						
						if(ProcessReStart[i].num >= DEF_RESTART_FAIL)
						{
							ProcStat.stat[i] = 1;
							dprintf(ERROR, "process restart 3.\n");
						}

						ProcLaunch(i);
					}
					else
					{
						ProcStat.stat[i] = 1;
					}
					com_shmem_write(id, &ProcStat, sizeof(ProcStat));
				}
			}
			
			if(ProcessReStart[i].num > DEF_RESTART_NONE)
			{
				ProcessReStart[i].time += DEF_MONIT_CYCLE;
			}
		}
		
		com_mtimer(ENUM_TIMER_PROC);
		time+=DEF_MONIT_CYCLE;
	}

	com_shmem_close(id);
	
	// 全プロセス停止
	ProcTerm();

	pthread_exit(NULL);
}

/*============================================================================*/
/*
 * @brief   term process
 * @note    kill process
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/13 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ProcTerm(void)
{
	int status;
	int ret = DEF_RET_OK;
	
	for(int i = 0; i < ProcNum; i++)
	{
		if(ProcessInfo[i].pid != -1 && ProcessInfo[i].pid != 0)
		{
			//プロセス終了
			if(kill(ProcessInfo[i].pid, SIGINT) == -1)
			{
dprintf(ERROR, "kill func failed. pid = %d. errno = %d\n", ProcessInfo[i].pid, errno);
				ret = DEF_RET_NG;
			}
			else
			{
				//終了待ち
				for(int j = 0; j < DEF_KILL_WAIT_NUM; j++)
				{
					if(waitpid(ProcessInfo[i].pid, &status, WNOHANG) == 0)
					{
						usleep(1000);
					}
					else
					{
						break;
					}
					
					if(j == DEF_KILL_WAIT_NUM - 1)
					{
dprintf(ERROR, "kill failed. pid = %d\n", ProcessInfo[i].pid);
						ret = DEF_RET_NG;
					}
				}
			}
		}
	}
	
	return ret;
}
