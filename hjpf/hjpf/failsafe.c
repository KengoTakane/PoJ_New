/* ************************************************************************** */
/* 
 * ファイル名           failsafe.c
 * コンポーネント名     フェールセーフ
 * 説明                 フェールセーフ処理
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/12
 * 作成者               高根
  */
/* ************************************************************************** */
/* ************************************************************************** */
/* pragma定義                                                                 */
/* ************************************************************************** */


/* ************************************************************************** */
/* include(システムヘッダ)                                                    */
/* ************************************************************************** */
#include <glib.h>
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

/* ************************************************************************** */
/* include(ユーザ定義ヘッダ)                                                  */
/* ************************************************************************** */
#include "debug.h"
#include "com_timer.h"
#include "com_shmem.h"
#include "failsafe.h"
#include "hjpf.h"
#include "gnss.h"
#include "ins.h"
#include "imu.h"
#include "altmt.h"
#include "ping.h"
#include "i2c.h"
#include "camera.h"
#include "mavlink.h"

/* ************************************************************************** */
/* マクロ定義                                                                 */
/* ************************************************************************** */


/* ************************************************************************** */
/* typedef 定義                                                               */
/* ************************************************************************** */


/* ************************************************************************** */
/* enum定義                                                                   */
/* ************************************************************************** */


/* ************************************************************************** */
/* struct/union定義                                                           */
/* ************************************************************************** */


/* ************************************************************************** */
/* static 変数宣言                                                            */
/* ************************************************************************** */
static failsafeInfo FsInfo;										/* 監視情報 */
static int32_t FsThresh[DEF_FS_ERR_MAX];						/* 故障閾値情報 */
static procStat FsInfoProc;										/* プロセスリソース情報 */
static resourceStat FsInfoResc;									/* リソースリソース情報 */
static gnssStat FsInfoGNSS;										/* GNSSリソース情報 */
static STR_INS_INFO FsInfoINS;									/* INSリソース情報 */
static STR_IMU_INFO FsInfoIMU;									/* IMUリソース情報 */
static STR_ALTMT_INFO FsInfoALTMT;								/* 高度計リソース情報 */
static STR_PING_INFO FsInfoPING[4];								/* Wifiリソース情報 */
static HMC6343 FsInfoMAG;										/* 磁気センサリソース情報 */
static BME680 FsInfoATM;										/* 大気圧計リソース情報 */
static cameraStat FsInfoCamera[6];									/* カメラリソース情報 */
static mavlinkRecv FsInfoMavlink;								/* キューブパイロット情報 */

static failsafeTable FsTable[] = {
	{ENUM_FS_PROC, DEF_PROC_SHMMNG_NAME, &FsInfoProc, sizeof(FsInfoProc), &FsThresh[0], &(FsInfoProc.stat[0]), &(FsInfo.proc)},	
	{ENUM_FS_CPU, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[1], &(FsInfoResc.cpu_load[0]), &(FsInfo.cpu_load[0])},
	{ENUM_FS_CPU1, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[2], &(FsInfoResc.cpu_load[1]), &(FsInfo.cpu_load[1])},
	{ENUM_FS_CPU2, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[3], &(FsInfoResc.cpu_load[2]), &(FsInfo.cpu_load[2])},
	{ENUM_FS_CPU3, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[4], &(FsInfoResc.cpu_load[3]), &(FsInfo.cpu_load[3])},
	{ENUM_FS_CPU4, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[5], &(FsInfoResc.cpu_load[4]), &(FsInfo.cpu_load[4])},
	{ENUM_FS_CPU5, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[6], &(FsInfoResc.cpu_load[5]), &(FsInfo.cpu_load[5])},
	{ENUM_FS_CPU6, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[7], &(FsInfoResc.cpu_load[6]), &(FsInfo.cpu_load[6])},
	{ENUM_FS_CPU7, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[8], &(FsInfoResc.cpu_load[7]), &(FsInfo.cpu_load[7])},
	{ENUM_FS_CPU8, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[9], &(FsInfoResc.cpu_load[8]), &(FsInfo.cpu_load[8])},
	{ENUM_FS_CPU9, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[10], &(FsInfoResc.cpu_load[9]), &(FsInfo.cpu_load[9])},
	{ENUM_FS_CPU10, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[11], &(FsInfoResc.cpu_load[10]), &(FsInfo.cpu_load[10])},
	{ENUM_FS_CPU11, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[12], &(FsInfoResc.cpu_load[11]), &(FsInfo.cpu_load[11])},
	{ENUM_FS_CPU12, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[13], &(FsInfoResc.cpu_load[12]), &(FsInfo.cpu_load[12])},
	{ENUM_FS_MEM, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[14], &(FsInfoResc.mem_load), &(FsInfo.mem)},
	{ENUM_FS_DISK, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[15], &(FsInfoResc.disk_load), &(FsInfo.disk)},
	{ENUM_FS_THERM, DEF_RES_SHMMNG_NAME, &FsInfoResc, sizeof(FsInfoResc), &FsThresh[16], &(FsInfoResc.cpu_therm), &(FsInfo.cpu_therm)},
	{ENUM_FS_CAMERA1, "/readcam0", &FsInfoCamera[0], sizeof(FsInfoCamera[0]), &FsThresh[17], &(FsInfoCamera[0].Stat), &(FsInfo.camera[0])},
	{ENUM_FS_CAMERA2, "/readcam1", &FsInfoCamera[1], sizeof(FsInfoCamera[1]), &FsThresh[18], &(FsInfoCamera[1].Stat), &(FsInfo.camera[1])},
	{ENUM_FS_CAMERA3, "/readcam2", &FsInfoCamera[2], sizeof(FsInfoCamera[2]), &FsThresh[19], &(FsInfoCamera[2].Stat), &(FsInfo.camera[2])},
	{ENUM_FS_CAMERA4, "/readcam3", &FsInfoCamera[3], sizeof(FsInfoCamera[3]), &FsThresh[20], &(FsInfoCamera[3].Stat), &(FsInfo.camera[3])},
	{ENUM_FS_CAMERA5, "/readcam4", &FsInfoCamera[4], sizeof(FsInfoCamera[4]), &FsThresh[21], &(FsInfoCamera[4].Stat), &(FsInfo.camera[4])},
	{ENUM_FS_CAMERA6, "/readcam5", &FsInfoCamera[5], sizeof(FsInfoCamera[5]), &FsThresh[22], &(FsInfoCamera[5].Stat), &(FsInfo.camera[5])},
	{ENUM_FS_ALTITUDE, DEF_ALTMT_SHMEM_NAME, &FsInfoALTMT, sizeof(FsInfoALTMT), &FsThresh[23], &(FsInfoALTMT.Stat), &(FsInfo.altitude)},
	{ENUM_FS_GNSS_TAKION, DEF_GNSS_SHMEM_NAME, &FsInfoGNSS, sizeof(FsInfoGNSS), &FsThresh[24], &(FsInfoGNSS.Stat), &(FsInfo.gnss_takion)},
	{ENUM_FS_INS, DEF_INS_SHMEM_NAME, &FsInfoINS, sizeof(FsInfoINS), &FsThresh[25], &(FsInfoINS.Stat), &(FsInfo.ins)},
	{ENUM_FS_IMU, DEF_IMU_SHMEM_NAME, &FsInfoIMU, sizeof(FsInfoIMU), &FsThresh[26], &(FsInfoIMU.Stat), &(FsInfo.imu)},
	{ENUM_FS_WIFI, "/wifi", &FsInfoPING[0], sizeof(FsInfoPING[0]), &FsThresh[27], &(FsInfoPING[0].Stat), &(FsInfo.wifi)},
	{ENUM_FS_ATM_PRESSURE, DEF_BME_SHMEM_NAME, &FsInfoATM, sizeof(FsInfoATM), &FsThresh[28], &(FsInfoATM.Stat), &(FsInfo.atm_pressure)},
	{ENUM_FS_ECU_JETSON1, "/ecu0", &FsInfoPING[1], sizeof(FsInfoPING[1]), &FsThresh[29], &(FsInfoPING[1].Stat), &(FsInfo.ecu_jetson[0])},
	{ENUM_FS_ECU_JETSON2, "/ecu1", &FsInfoPING[2], sizeof(FsInfoPING[2]), &FsThresh[30], &(FsInfoPING[2].Stat), &(FsInfo.ecu_jetson[1])},
	{ENUM_FS_ECU_JETSON3, "/ecu2", &FsInfoPING[3], sizeof(FsInfoPING[3]), &FsThresh[31], &(FsInfoPING[3].Stat), &(FsInfo.ecu_jetson[2])},
	{ENUM_FS_MAG, DEF_HMC_SHMEM_NAME, &FsInfoMAG, sizeof(FsInfoMAG), &FsThresh[32], &(FsInfoMAG.Stat), &(FsInfo.mag)},
	{ENUM_FS_ECU, "/mavlink_recv", &FsInfoMavlink, sizeof(FsInfoMavlink), &FsThresh[33], &(FsInfoMavlink.Stat), &(FsInfo.ecu)},
	//{ENUM_FS_ECU, &FsThresh[26]},
};				    /* 監視一覧表 */


/* ************************************************************************** */
/* global 変数宣言                                                            */
/* ************************************************************************** */
extern int gComm_StopFlg;

/* ************************************************************************** */
/* プロトタイプ宣言(内部関数)                                                 */
/* ************************************************************************** */
static int32_t FailsafeConf(char aFilename[]);
static int32_t FailsafeJudge(int32_t aIndex);
static int32_t FailsafeGetID(ENUM_FS_ERRCODE aErrcode);


/* ************************************************************************** */
/* 関数定義                                                                   */
/* ************************************************************************** */

/* ************************************************************************** */
/* 
 * 関数名   設定ファイル読込
 * 機能     フェールセーフ設定ファイルを読み込む．
 * 引数:    aFilename：[i] 設定ファイル名
 * 戻り値:  0：正常終了，-1：エラー
 * 作成日   2023/12/12 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t FailsafeConf(char aFilename[])
{
    int32_t ret = DEF_FS_TRUE;	/* 戻り値変数 */
    int32_t tErrNum = 0;        /* エラー一覧数 */
	GKeyFile* tFSKeyFile;
	GError* err = NULL;
	gchar** tGroupArray;
	gsize group_size;

	tFSKeyFile = g_key_file_new();	/* ファイルオブジェクト作成 */

	if (!g_key_file_load_from_file(tFSKeyFile, aFilename, 0, &err))	/* コンフィグファイルのオープン */
	{
		dprintf(ERROR, "load failsafe conf file failed. filename = %s\n", aFilename);
		ret = DEF_FS_FALSE;
	}
	else
	{
		tGroupArray = g_key_file_get_groups(tFSKeyFile, &group_size);	/* グループ数を取得 */
		tErrNum = (int32_t)group_size;	/* エラー一覧数に変換 */

		for (int cnt = 0; cnt < tErrNum; cnt++)
		{
            /* 故障閾値を取得 */
			FsThresh[cnt] = (int32_t)g_key_file_get_integer(tFSKeyFile, tGroupArray[cnt], "Thresh", &err);

			if ((DEF_FS_FALSE > FsThresh[cnt]) || (NULL != err))
			{
				dprintf(ERROR, "failed to parse thresh(No.%d).\n", cnt);
				ret = DEF_FS_FALSE;
			}
		}
		g_strfreev(tGroupArray);
	}

	g_key_file_free(tFSKeyFile);	/* ファイルの開放処理 */

	if (NULL != err)
	{
		g_error_free(err);
	}

	return ret;
}

/* ************************************************************************** */
/* 
 * 関数名   メイン処理
 * 機能     プロセス，デバイスの正常・異常判定を共有メモリに書き込む．
 * 引数:    arg：[i] 引数
 * 戻り値:  なし
 * 作成日   2023/12/12 [0.0.1] 新規作成
 */
/* ************************************************************************** */
void* FailsafeMain(void* arg)
{
    int32_t ret;
	int32_t tShmemID;
	int32_t tFsShmID;
	int32_t index;

	com_timer_init(ENUM_TIMER_FAILSAFE, 100);

    /* 設定ファイル読み込み */
    ret = FailsafeConf(arg);
    if(ret == DEF_RET_NG){
		dprintf(ERROR, "file load failed\n");
		pthread_exit(NULL);
	}

	/* フェールセーフの共有メモリオープン */
	tFsShmID = com_shmem_open(DEF_FS_SHMEM_NAME, SHM_KIND_PLATFORM);
	if(tFsShmID == DEF_COM_SHMEM_FALSE)
    {
        dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_FS_SHMEM_NAME);
        pthread_exit(NULL);
    }

	/* フェールセーフ */
	while (gComm_StopFlg == DEF_COMM_OFF)
	{
		/* 設定フェールセーフ数まで */
		for(uint32_t errocode = 1; errocode <= DEF_FS_ERR_MAX; errocode++)
		{
			/* リソース情報取得 */
			index = FailsafeGetID(errocode);
			if (index != DEF_FS_FALSE) {
				tShmemID = com_shmem_open(FsTable[index].shmname, SHM_KIND_PLATFORM);
				if(tShmemID == -1)
				{
					// dprintf(WARN, "com_shmem_open(%s) error\n", FsTable[index].shmname);
					continue;
				}
				//printf("errorcode=%d, index=%d, tShmemID=%d\n", errocode, index, tShmemID);
				com_shmem_read(tShmemID, FsTable[index].resDataInfo, FsTable[index].size);
				com_shmem_close(tShmemID);

				/* 故障レベル判定 */
				*(FsTable[index].data) = FailsafeJudge(index);
				//printf("errorcode=%d , fail level = %d\n", errocode, *FsTable[index].data);
				/* 共有メモリ書き込み */
				com_shmem_write(tFsShmID, &FsInfo, sizeof(FsInfo));
			}
		}
		com_mtimer(ENUM_TIMER_FAILSAFE);
	}

	/* 共有メモリクローズ */
    com_shmem_close(tFsShmID);
	pthread_exit(NULL);
	
}

/* ************************************************************************** */
/* 
 * 関数名   故障判定
 * 機能     エラーコードの故障レベルを判定する．
 * 引数:    aIndex：[i] インデックス番号
 * 戻り値:   -1：失敗，0以上：故障レベル
 * 作成日   2023/12/12 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t FailsafeJudge(int32_t aIndex)
{
	int32_t ret = DEF_FS_FALSE;

	if (*(FsTable[aIndex].resData) > *(FsTable[aIndex].thresh))
	{
		ret = DEF_FS_FAIL_NOW;	/* 現在故障 */
	}
	else if (*(FsTable[aIndex].data) != DEF_FS_SAFE)
	{
		ret = DEF_FS_FAIL_PAST;	/* 過去故障 */
	}
	else
	{
		ret = DEF_FS_SAFE;	/* 無故障 */
	}
	
	return ret;
}

/* ************************************************************************** */
/* 
 * 関数名   インデックス番号取得
 * 機能     エラーコードからテーブルのインデックス番号を取得する．
 * 引数:    aErrcode：[i] エラーコード
 * 戻り値:   -1：失敗，0以上：インデックス番号
 * 作成日   2023/12/12 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t FailsafeGetID(ENUM_FS_ERRCODE aErrcode)
{
	int32_t ret = DEF_FS_FALSE;

	for (int32_t cnt = 0; cnt < DEF_FS_ERR_MAX; cnt++)
	{
		if (aErrcode == FsTable[cnt].errcode)
		{
			ret = cnt;
			break;
		}
		
	}
	return ret;
}


