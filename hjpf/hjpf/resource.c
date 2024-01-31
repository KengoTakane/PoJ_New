/*============================================================================*/
/*
 * @file    resource.c
 * @brief   リソース管理
 * @note    リソース管理
 * @date    2023/11/27
 */
/*============================================================================*/

/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"
#include "resource.h"
#include "gnss.h"
#include "i2c.h"
#include "ins.h"
#include "imu.h"
#include "altmt.h"
#include "camera.h"
#include "ping.h"
#include "mavlink.h"

/*============================================================================*/
/* global */
/*============================================================================*/
static resourceInfo ResourceInfo;
static resourceStat ResourceStat;
extern int gComm_StopFlg;
extern pthread_mutex_t g_mutex;   
static resUARTInfo	g_uartGNSS;
static resUARTInfo	g_uartINS;
static resUARTInfo	g_uartIMU;
static resUARTInfo	g_uartALTMT;
static resI2CInfo	g_i2cBME_HMC;
static resPingInfo	g_Ping;
static resMavlinkInfo g_Mavlink;
static resCameraInfo g_Camera[6];

static int g_uartGNSSRun = 0;
static int g_uartINSRun = 0;
static int g_uartIMURun = 0;
static int g_uartALTMTRun = 0;
static int g_i2cBME_HMCRun = 0;
static int g_PingRun = 0;
static int g_MavlinkRun = 0;
static int g_CameraRun[6] = { 0 };

static confSectionTbl ConfSectionTble[RES_KIND_MAX] = {
	{RES_KIND_CPU_LOAD, "cpu_load"},
	{RES_KIND_MEM_LOAD, "mem_load"},
	{RES_KIND_DISK_LOAD, "disk_load"},
	{RES_KIND_CPU_THERM, "cpu_therm"},
};

static resThreadInfo g_res_threadInfo[] = {
	{ GNSSMain, &g_uartGNSS, -1, &g_uartGNSSRun},
	{ ins_serial_main, &g_uartINS, -1, &g_uartINSRun},
	{ imu_serial_main, &g_uartIMU, -1, &g_uartIMURun},
	{ altmt_serial_main, &g_uartALTMT, -1, &g_uartALTMTRun},
	{ i2c_main, &g_i2cBME_HMC, -1, &g_i2cBME_HMCRun},
	{ ping_main, &g_Ping, -1, &g_PingRun},
	{ MavlinkMain, &g_Mavlink, -1, &g_MavlinkRun},
	{ CameraMain, &g_Camera[0], -1, &g_CameraRun[0]},
	{ CameraMain, &g_Camera[1], -1, &g_CameraRun[1]},
	{ CameraMain, &g_Camera[2], -1, &g_CameraRun[2]},
	{ CameraMain, &g_Camera[3], -1, &g_CameraRun[3]},
	{ CameraMain, &g_Camera[4], -1, &g_CameraRun[4]},
	{ CameraMain, &g_Camera[5], -1, &g_CameraRun[5]},
};

/*============================================================================*/
/* prototype */
/*============================================================================*/
static int ResCPUTherm(void);
static int ResDiskLoad(void);
static int ResMemLoad(void);
static int ResCPULoad(void);
static int ResReadFile(char filename[]);
static int ResCheckDevice(char* ResGroupName);

/*============================================================================*/
/* const */
/*============================================================================*/



/*============================================================================*/
/*
 * @brief   CPU温度取得処理
 * @note    CPUの温度を取得する
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ResCPUTherm(void)
{
	int ret = DEF_RET_OK;
	int fd;
	char str[DEF_STR_MAX];
	
	//GPU温度を取得
	fd = open("/sys/devices/virtual/thermal/thermal_zone0/temp", O_RDONLY);
	if(fd == -1)
	{
dprintf(ERROR, "gpu load failed.\n");
		ret = DEF_RET_NG;
	}
	else
	{
		read(fd, str, sizeof(str));
		ResourceStat.cpu_therm = strtol(str, NULL, DEF_DECIMAL);
	}
	close(fd);
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   ディスク使用量取得処理
 * @note    ディスク使用量を取得する
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ResDiskLoad(void)
{
	int ret = DEF_RET_OK;
	char str[DEF_STR_MAX];
	FILE *fp;
	
	char filesystem[DEF_STR_MAX];
	char use[DEF_STR_MAX];
	int blocks, used, available;
	
	//dfコマンド結果を取得
	fp = popen("df", "r");
	if(fp == NULL)
	{
dprintf(ERROR, "mem load failed.\n");
		ret = DEF_RET_NG;
	}
	else
	{
		//ディスク使用量を取得
		fgets(str, sizeof(str), fp);
		fgets(str, sizeof(str), fp);
		sscanf(str, "%s %d %d %d %s",
					filesystem,
					&blocks,
					&used,
					&available,
					use
		);
		
		ResourceStat.disk_load = strtol(use, NULL, DEF_DECIMAL);
	}
	
	pclose(fp);
	
	return ret;
}


/*============================================================================*/
/*
 * @brief   メモリ使用量取得処理
 * @note    メモリ使用量を取得する
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ResMemLoad(void)
{
	int ret = DEF_RET_OK;
	double mem_stat[MEMINFO_KIND_MAX];
	char str[DEF_STR_MAX];
	int j;
	FILE *fp;
	
	// /proc/meminfoを取得
	fp = fopen("/proc/meminfo", "r");
	if(fp == NULL)
	{
dprintf(ERROR, "mem load failed.\n");
		ret = DEF_RET_NG;
	}
	else
	{
		//必要情報を取得
		for(int i = 0; i < MEMINFO_KIND_MAX; i++)
		{
			fgets(str, sizeof(str), fp);
			
			j = 0;
			while(!isdigit(str[j]))
			{
				j++;
			}
			
			mem_stat[i] = strtol(str + j, NULL, DEF_DECIMAL);
		}
		
		//メモリ使用量を計算
		ResourceStat.mem_load = (mem_stat[MEMINFO_KIND_TOTAL] - mem_stat[MEMINFO_KIND_FREE] 
						- mem_stat[MEMINFO_KIND_BUF] - mem_stat[MEMINFO_KIND_CASHE]) * 100 / mem_stat[MEMINFO_KIND_TOTAL];
	}
	
	fclose(fp);
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   CPU負荷取得処理
 * @note    CPU負荷を取得する
 * @param   引数  : void
 *                  
 * @return  戻り値: int
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ResCPULoad(void)
{
	int ret = DEF_RET_OK;
	
	static linuxProcStat prev[DEF_CPU_NUM + 1];
	linuxProcStat curr[DEF_CPU_NUM + 1];
	char buf[DEF_STR_MAX];
	int total;
	double idle_load;
	FILE *fp;
	
	// /proc/statをオープン
	fp = fopen("/proc/stat", "r");
	if(fp == NULL)
	{
dprintf(ERROR, "cpu load failed.\n");
		ret = DEF_RET_NG;
	}
	else
	{
		//必要情報を取得
		for(int i = 0; i < (DEF_CPU_NUM + 1); i++)
		{
			fgets(buf, sizeof(buf), fp);
			sscanf(buf, "%s %d %d %d %d %d %d %d %d %d %d",
					curr[i].name,
					&curr[i].user,
					&curr[i].nice,
					&curr[i].system,
					&curr[i].idle,
					&curr[i].iowait,
					&curr[i].irq,
					&curr[i].softirq,
					&curr[i].steal,
					&curr[i].guest,
					&curr[i].guest_nice
				);
			
			//cpu負荷を計算
			total = (curr[i].user - prev[i].user) +
					(curr[i].nice - prev[i].nice) +
					(curr[i].system - prev[i].system) +
					(curr[i].idle - prev[i].idle);
					
			idle_load = ((double)curr[i].idle - (double)prev[i].idle) * 100 / (double)total;
			ResourceStat.cpu_load[i] = 100 - idle_load;
			prev[i] = curr[i];
		}
	}
	
	fclose(fp);
	
	return ret;
}


/*============================================================================*/
/*
 * @brief   設定ファイル読み込み処理
 * @note    設定ファイルを読み込む
 * @param   引数  : char[]
 *                  
 * @return  戻り値: int
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ResReadFile(char filename[])
{
	int ret = DEF_RET_OK;
	int tmp_value;
	GKeyFile *file;
	GError *err = NULL;
	gchar* char_tmp_value;
	gchar** tGroupArray;
	gsize group_size;
	int32_t sResNum;
	
	file = g_key_file_new();
	
	//ファイルオープン
	if(!g_key_file_load_from_file(file, filename, 0, &err))
	{
		dprintf(ERROR, "load resouce conf file failed. filename = %s. err = %s\n", filename, err->message);
		ret = DEF_RET_NG;
	}
	else
	{
		tGroupArray = g_key_file_get_groups(file, &group_size);	/* 設定されているリソースグループ数を取得 */
		sResNum = (int32_t)group_size;	/* 型変換 */
		char ResGroupList[sResNum][128];

		for (int cnt = 0; cnt < sResNum; cnt++)
		{
			strcpy(ResGroupList[cnt], (char*)tGroupArray[cnt]);	/* リソースグループ名を取得 */
			ResCheckDevice(ResGroupList[cnt]);
		}
		

		//リソース監視周期を取得
		for(int i = 0; i < RES_KIND_MAX; i++)
		{
			if(0 == (tmp_value = g_key_file_get_integer(file, ConfSectionTble[i].item, "period", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get prriod[%d] failed. %s\n", i, err->message);
				ret = DEF_RET_NG;
			}
			else
			{
				ResourceInfo.period[i] = tmp_value;
				
				if(ResourceInfo.period[i] < DEF_PERIOD_MIN || ResourceInfo.period[i] % DEF_MONIT_CYCLE != 0)
				{
					dprintf(ERROR, "period[%d] failed. %d\n", i, ResourceInfo.period[i]);
					ret = DEF_RET_NG;
				}
			}
		}

		//GNSS
		if (g_uartGNSSRun == 1)
		{
			if(NULL == (char_tmp_value = g_key_file_get_string(file, "GNSS", "devname", &err)))
			{
				g_uartGNSSRun = 0;
				dprintf(ERROR, "get gnss failed. %s\n",err->message);
				//ret = DEF_RET_NG;
			}
			else
			{
				g_uartGNSSRun = 1;
				strcpy(g_uartGNSS.devname, (char*)char_tmp_value);
			}
			if(0 == (g_uartGNSS.timeout = g_key_file_get_integer(file, "GNSS", "timeout", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get gnss timeout failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
		}
		
		

		//INS
		if (g_uartINSRun == 1)
		{
			if(NULL == (char_tmp_value = g_key_file_get_string(file, "INS", "devname", &err)))
			{
				g_uartINSRun = 0;
				dprintf(ERROR, "get ins failed. %s\n",err->message);
				//ret = DEF_RET_NG;
			}
			else
			{
				g_uartINSRun = 1;
				strcpy(g_uartINS.devname, (char*)char_tmp_value);
			}
			if(0 == (g_uartINS.timeout = g_key_file_get_integer(file, "INS", "timeout", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get ins timeout failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
		}
		

		//IMU
		if (g_uartIMURun == 1)
		{
			if(NULL == (char_tmp_value = g_key_file_get_string(file, "IMU", "devname", &err)))
			{
				g_uartIMURun = 0;
				dprintf(ERROR, "get ins failed. %s\n",err->message);
				//ret = DEF_RET_NG;
			}
			else
			{
				g_uartIMURun = 1;
				strcpy(g_uartIMU.devname, (char*)char_tmp_value);
			}
			if(0 == (g_uartIMU.timeout = g_key_file_get_integer(file, "IMU", "timeout", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get imu timeout failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
		}
		
		

		//ALTMT
		if (g_uartALTMTRun == 1)
		{
			if(NULL == (char_tmp_value = g_key_file_get_string(file, "ALTMT", "devname", &err)))
			{
				g_uartALTMTRun = 0;
				dprintf(ERROR, "get ins failed. %s\n",err->message);
				//ret = DEF_RET_NG;
			}
			else
			{
				if(strlen(char_tmp_value) > 0)
				{
					g_uartALTMTRun = 1;
					strcpy(g_uartALTMT.devname, (char*)char_tmp_value);	
				}
			}

			if(0 == (g_uartALTMT.timeout = g_key_file_get_integer(file, "ALTMT", "timeout", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get altmt timeout failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
		}
		
		

		//BME_HMC
		if (g_i2cBME_HMCRun == 1)
		{
			if(NULL == (char_tmp_value = g_key_file_get_string(file, "BME_HMC", "devname", &err)))
			{
				g_i2cBME_HMCRun = 0;
				dprintf(ERROR, "get ins failed. %s\n",err->message);
				//ret = DEF_RET_NG;
			}
			else
			{
				g_i2cBME_HMCRun = 1;
				strcpy(g_i2cBME_HMC.devname, (char*)char_tmp_value);
			}

			if(0 == (g_i2cBME_HMC.period = g_key_file_get_integer(file, "BME_HMC", "period", &err)) && (NULL != err))
			{
				dprintf(ERROR, "load cpu failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
			else
			{
				if(g_i2cBME_HMC.period < DEF_PERIOD_MIN || g_i2cBME_HMC.period % DEF_MONIT_CYCLE != 0)
				{
					dprintf(ERROR, "i2c period failed. %d\n", g_i2cBME_HMC.period);
					ret = DEF_RET_NG;
				}
			}
			if(0 == (g_i2cBME_HMC.timeout = g_key_file_get_integer(file, "BME_HMC", "timeout", &err)) && (NULL != err))
			{
				dprintf(ERROR, "load i2c timeout failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
			else
			{
				if(g_i2cBME_HMC.timeout < 0)
				{
					dprintf(ERROR, "i2c timeout failed. %d\n", g_i2cBME_HMC.timeout);
					ret = DEF_RET_NG;
				}
			}
		}
		
		

		//PING
		if (g_PingRun == 1)
		{
			if(0 == (g_Ping.period = g_key_file_get_integer(file, "PING", "period", &err)) && (NULL != err))
			{
				dprintf(ERROR, "load cpu failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
			else
			{
				if(g_Ping.period < DEF_PERIOD_MIN || g_Ping.period % DEF_MONIT_CYCLE != 0)
				{
					dprintf(ERROR, "i2c period failed. %d\n", g_Ping.period);
					ret = DEF_RET_NG;
				}
			}

			for(int i = 0; i < DEF_PING_MAX; i++)
			{
				char shm_name[64] = "shmname";
				char host_name[64] = "hostname";
				char char_i[32];

				sprintf(char_i, "%d", i);
				strcat(shm_name, char_i);
				strcat(host_name, char_i);

				if(NULL == (char_tmp_value = g_key_file_get_string(file, "PING", shm_name, &err)))
				{
					g_Ping.pingNum = i;
					//dprintf(ERROR, "get ins failed. %s\n",err->message);
					//ret = DEF_RET_NG;
					break;
				}
				else
				{
					strcpy(g_Ping.shmname[i], (char*)char_tmp_value);
				}

				if(NULL == (char_tmp_value = g_key_file_get_string(file, "PING", host_name, &err)))
				{
					g_Ping.pingNum = i;
					//dprintf(ERROR, "get ins failed. %s\n",err->message);
					//ret = DEF_RET_NG;
					break;
				}
				else
				{
					strcpy(g_Ping.hostname[i], (char*)char_tmp_value);
				}

				g_free(char_tmp_value);
			}
		}
		
		
		
		//Mavlink
		if (g_MavlinkRun == 1)
		{
			if(NULL == (char_tmp_value = g_key_file_get_string(file, "MAVLINK", "devname", &err)))
			{
				g_MavlinkRun = 0;
				dprintf(ERROR, "get mavlink devname failed. %s\n",err->message);
				//ret = DEF_RET_NG;
			}
			else
			{
				g_MavlinkRun = 1;
				strcpy(g_Mavlink.devname, (char*)char_tmp_value);
			}

			if(0 == (g_Mavlink.baudrate = g_key_file_get_integer(file, "MAVLINK", "baudrate", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get mavlink baudrate failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
			else
			{
				if(g_Mavlink.baudrate < 0)
				{
					dprintf(ERROR, " mavlink baudrate = %d. error.\n", g_Mavlink.baudrate);
					ret = DEF_RET_NG;
				}
			}

			/*
			if(0 == (g_Mavlink.read_period = g_key_file_get_integer(file, "MAVLINK", "read_period", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get mavlink read_period failed. %s\n", err->message);
				ret = DEF_RET_NG;
			}
			else
			{
				if(g_Mavlink.read_period < 0)
				{
					dprintf(ERROR, " mavlink read_period = %d. error.\n", g_Mavlink.read_period);
					ret = DEF_RET_NG;
				}
			}
			*/

			if(0 == (g_Mavlink.write_period = g_key_file_get_integer(file, "MAVLINK", "period", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get mavlink write_period failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
			else
			{
				if(g_Mavlink.write_period < 0)
				{
					dprintf(ERROR, " mavlink write_period = %d. error.\n", g_Mavlink.write_period);
					ret = DEF_RET_NG;
				}
			}

			if(0 == (g_Mavlink.timeout = g_key_file_get_integer(file, "MAVLINK", "timeout", &err)) && (NULL != err))
			{
				dprintf(ERROR, "get mavlink timeout failed. %s\n", err->message);
				// ret = DEF_RET_NG;
			}
			else
			{
				if(g_Mavlink.timeout < 0)
				{
					dprintf(ERROR, " mavlink timeout = %d. error.\n", g_Mavlink.timeout);
					ret = DEF_RET_NG;
				}
			}
		}
		
		

		//Camera
		for(int i = 0; i < 6; i++)
		{
			if (g_CameraRun[i] == 1)
			{
				char camera_section_name[128];
				sprintf(camera_section_name, "%s%d", "Camera", i);
				if(NULL == (char_tmp_value = g_key_file_get_string(file, camera_section_name, "devname", &err)))
				{
					g_CameraRun[i] = 0;
					dprintf(ERROR, "get %s devname failed. %s\n", camera_section_name, err->message);
					//ret = DEF_RET_NG;
				}
				else
				{
					g_CameraRun[i] = 1;
					strcpy(g_Camera[i].devname, (char*)char_tmp_value);
					g_Camera[i].cameranum = i;
				}

				if(0 == (g_Camera[i].period = g_key_file_get_integer(file, camera_section_name, "period", &err)) && (NULL != err))
				{
					dprintf(ERROR, "get %s period failed. %s\n", camera_section_name, err->message);
					// ret = DEF_RET_NG;
				}
				else
				{
					if(g_Camera[i].period < 0)
					{
						dprintf(ERROR, " %s period = %d. error.\n", camera_section_name, g_Camera[i].period);
						ret = DEF_RET_NG;
					}
				}

				if(0 == (g_Camera[i].timeout = g_key_file_get_integer(file, camera_section_name, "timeout", &err)) && (NULL != err))
				{
					dprintf(ERROR, "get %s timeout failed. %s\n", camera_section_name, err->message);
					// ret = DEF_RET_NG;
				}
				else
				{
					if(g_Camera[i].timeout < 0)
					{
						dprintf(ERROR, " %s timeout = %d. error.\n", camera_section_name, g_Camera[i].timeout);
						ret = DEF_RET_NG;
					}
				}
			}
			
			
		}
		
	}
	
	//メモリ開放
	g_key_file_free(file);
	if(NULL != err)
	{
		g_error_free(err);
	} 
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   main処理
 * @note    main
 * @param   引数  : void*
 *                  
 * @return  戻り値: void*
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
void* ResMain(void* arg){
	int ret;
	int id;
	int time = 0;
	
	//設定ファイル読み込み
	ret = ResReadFile(arg);
	if(ret == DEF_RET_NG){
		dprintf(ERROR, "file load failed\n");
		pthread_exit(NULL);
	}

#if DEF_RES_TEST
	printf("cpu_load_period = %d\n", ResourceInfo.period[0]);
	printf("mem_load_period = %d\n", ResourceInfo.period[1]);
	printf("disk_load_period = %d\n", ResourceInfo.period[2]);
	printf("cpu_therm_period = %d\n", ResourceInfo.period[3]);
#endif
	
	com_timer_init(ENUM_TIMER_RES, DEF_MONIT_CYCLE);
	id = com_shmem_open(DEF_RES_SHMMNG_NAME, SHM_KIND_PLATFORM);
	if(id == DEF_COM_SHMEM_FALSE){
                dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_RES_SHMMNG_NAME);
		pthread_exit(NULL);
	}	

	// スレッド生成
	for (int cnt = 0; cnt < sizeof(g_res_threadInfo) / sizeof(resThreadInfo); cnt++) 
	{
		if(*(g_res_threadInfo[cnt].isStart) == 1)
		{
			ret = pthread_create(&g_res_threadInfo[cnt].threadid, NULL, g_res_threadInfo[cnt].thread, g_res_threadInfo[cnt].arg);
			//printf("[%d]thread id = %ld\n", cnt, g_res_threadInfo[cnt].threadid);
			if (ret != DEF_RET_OK) 
			{
				dprintf(WARN, "pthread_create(%d) error=%d\n", cnt, errno);
			}
		}
		else
		{
			dprintf(INFO, "[%d] isstart = 0\n", cnt);
		}
	}
	
	//リソース監視
#if 1
	while(gComm_StopFlg == DEF_COMM_OFF)
	{
		for(int i = 0; i < RES_KIND_MAX; i++)
		{
			if(ResourceInfo.period[i] != DEF_PERIOD_MIN && time % ResourceInfo.period[i] == 0)
			{
				switch(i)
				{
					case RES_KIND_CPU_LOAD:
						ret = ResCPULoad();
						break;
					case RES_KIND_MEM_LOAD:
						ret = ResMemLoad();
						break;
					case RES_KIND_DISK_LOAD:
						ret = ResDiskLoad();
						break;
					case RES_KIND_CPU_THERM:
						ret = ResCPUTherm();
						break;
					default:
dprintf(ERROR, "cannot get resource data.\n");
				}

				if(ret == DEF_RET_OK)
				{
					//共有メモリに書き込み
					com_shmem_write(id, &ResourceStat, sizeof(ResourceStat));		
				}
#if DEF_RES_TEST			
				for(int i = 0; i < 13; i++)
				{
					printf("cpu_load[%d] = %d\n", i, ResourceStat.cpu_load[i]);
				}

				printf("mem_load = %d\n", ResourceStat.mem_load);
				printf("disk_load = %d\n", ResourceStat.disk_load);
				printf("cpu_therm = %d\n", ResourceStat.cpu_therm);
#endif
			}
		}		
		com_mtimer(ENUM_TIMER_RES);
		time += DEF_MONIT_CYCLE;
	}
#endif

	//スレッド終了待ち
	for (int cnt = 0; cnt < sizeof(g_res_threadInfo) / sizeof(resThreadInfo); cnt++) 
	{
		if(*(g_res_threadInfo[cnt].isStart) == 1)
		{
			void *pret;
			ret = pthread_join(g_res_threadInfo[cnt].threadid, &pret);
			if (ret != DEF_RET_OK) {
				dprintf(WARN, "pthread_join error=%d\n", errno);
			}
		}
	}
	
	com_shmem_close(id);
	pthread_exit(NULL);
}

/*============================================================================*/
/*
 * @brief   シリアル通信オープン
 * @note    シグナル通信開始
 * @param   引数  : デバイス名、ボーレート、データ長
 * @return  戻り値: ファイルディスクリプタ
 * @date    2019/12/23 [0.0.1] 新規作成
 */
/*============================================================================*/
int com_serial_open(char *devname, int BaudRate)
{	
	dprintf(INFO, "com_serial_open(%s, %d)\n", devname, BaudRate);

	int fd = 0;
	struct termios tio;

//	pthread_mutex_lock(&g_mutex);
	memset(&tio, 0x00, sizeof(tio));

//	fd = open(devname,O_RDWR | O_NOCTTY | O_NDELAY);
//	dprintf(WARN, "com_serial_open(%s,%d)\n", devname, BaudRate);
	fd = open(devname,O_RDWR);
	if(fd < 0){
		dprintf(ERROR, "open(%s) error=%d\n", devname, errno);
		pthread_mutex_unlock(&g_mutex);
		return -1;
	}
	speed_t speed;

	tio.c_cflag = CREAD | CLOCAL | CS8;

	/*
	tio.c_cflag += CREAD;  //受信有効
	tio.c_cflag += CLOCAL; //ローカルライン
	tio.c_cflag += CS8;    //データビット8bit
	tio.c_cflag += 1;      //ストップビット1bit
	tio.c_cflag += 0;      //パリティNone
	*/

	speed = (speed_t)BaudRate;

	//ボーレート設定
	cfsetispeed(&tio, speed);
	cfsetospeed(&tio, speed);

	//ボード設定
	ioctl(fd, TCSETS, &tio);

	dprintf(INFO, "serial dev open=[%s]\n", devname);
//	pthread_mutex_unlock(&g_mutex);

	return fd;
}


/*============================================================================*/
/*
 * @brief   シリアル通信クローズ
 * @note    シグナル通信終了
 * @param   引数  : デバイス名、ボーレート、データ長
 * @return  戻り値:
 * @date    2019/12/23 [0.0.1] 新規作成
 */
/*============================================================================*/
void com_serial_close(int fd)
{
	dprintf(INFO, "com_serial_close(%d)\n", fd);
	if(fd != -1)
	{
		close(fd);
	}
}

/*============================================================================*/
/*
 * @brief   区切り文字で分割
 * @note    区切り文字で分割
 * @param   引数  : 文字列、区切り文字
 * @return  戻り値:文字列の先頭ポインタ
 * @date    2020/01/21 [0.0.1] 新規作成
 */
/*============================================================================*/
char *strtoks(char *s1, const char *s2) {
    static char *str = 0;
    register int i, j;

    if (s1) {
        str = s1;
    } else {
        s1 = str;
    }
    if (!s1) { return(0); }

    j = strlen(s2);

    while (1) {
        if (!*str) {
            str = 0;
            return(s1);
        }

        for (i = 0; i < j; i++) {
            if (*str == s2[i]) {
                *str++ = 0;
                return(s1);
            }
        }

        str++;
    }
}

/*============================================================================*/
/*
 * @brief   デバイス情報の取得チェック
 * @note    リソース情報取得の対象となるデバイスが，リソース設定ファイルで設定されているか確認する．
 * @param   引数  : リソースのグループ名
 *                  
 * @return  戻り値: int
 * @date    2023/11/27 [0.0.1] 新規作成
 */
/*============================================================================*/
static int ResCheckDevice(char* ResGroupName)
{
	int32_t ret = DEF_COM_SHMEM_FALSE;

		if (strcmp(ResGroupName, "GNSS") == 0)
		{
			g_uartGNSSRun = 1;
		}
		else if (strcmp(ResGroupName, "INS") == 0)
		{
			g_uartINSRun = 1;
		}
		else if (strcmp(ResGroupName, "IMU") == 0)
		{
			g_uartIMURun = 1;
		}
		else if (strcmp(ResGroupName, "ALTMT") == 0)
		{
			g_uartALTMTRun = 1;
		}
		else if (strcmp(ResGroupName, "BME_HMC") == 0)
		{
			g_i2cBME_HMCRun = 1;
		}
		else if (strcmp(ResGroupName, "PING") == 0)
		{
			g_PingRun = 1;
		}
		else if (strcmp(ResGroupName, "MAVLINK") == 0)
		{
			g_MavlinkRun = 1;
		}
		else
		{
			for(int i = 0; i < 6; i++)
			{
				char camera_section_name[128];
				sprintf(camera_section_name, "%s%d", "Camera", i);
				if (strcmp(ResGroupName, camera_section_name) == 0)
				{
					g_CameraRun[i] = 1;
				}
				
			}
		}
	return ret;
}

