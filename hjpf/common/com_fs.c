/* ************************************************************************** */
/* 
 * ファイル名           com_fs.c
 * コンポーネント名     フェールセーフ
 * 説明                 フェールセーフ処理
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/13
 * 作成者               高根
  */
/* ************************************************************************** */
/* ************************************************************************** */
/* pragma定義                                                                 */
/* ************************************************************************** */


/* ************************************************************************** */
/* include(システムヘッダ)                                                    */
/* ************************************************************************** */
#include <stdint.h>
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
#include "com_fs.h"
#include "debug.h"
#include "com_timer.h"
#include "com_shmem.h"

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
static failsafeInfo FsInfo;										/*  */
static errorTable FsState[] = {
    {ENUM_FS_PROC, &(FsInfo.proc)},
	{ENUM_FS_CPU, &(FsInfo.cpu_load[0])},
	{ENUM_FS_CPU1, &(FsInfo.cpu_load[1])},
	{ENUM_FS_CPU2, &(FsInfo.cpu_load[2])},
	{ENUM_FS_CPU3, &(FsInfo.cpu_load[3])},
	{ENUM_FS_CPU4, &(FsInfo.cpu_load[4])},
	{ENUM_FS_CPU5, &(FsInfo.cpu_load[5])},
	{ENUM_FS_CPU6, &(FsInfo.cpu_load[6])},
	{ENUM_FS_CPU7, &(FsInfo.cpu_load[7])},
	{ENUM_FS_CPU8, &(FsInfo.cpu_load[8])},
	{ENUM_FS_CPU9,&(FsInfo.cpu_load[9])},
	{ENUM_FS_CPU10,&(FsInfo.cpu_load[10])},
	{ENUM_FS_CPU11,&(FsInfo.cpu_load[11])},
	{ENUM_FS_CPU12,&(FsInfo.cpu_load[12])},
	{ENUM_FS_MEM, &(FsInfo.mem)},
	{ENUM_FS_DISK, &(FsInfo.disk)},
	{ENUM_FS_THERM, &(FsInfo.cpu_therm)},
	{ENUM_FS_CAMERA1, &(FsInfo.camera[0])},
	{ENUM_FS_CAMERA2, &(FsInfo.camera[1])},
	{ENUM_FS_CAMERA3, &(FsInfo.camera[2])},
	{ENUM_FS_CAMERA4, &(FsInfo.camera[3])},
	{ENUM_FS_CAMERA5, &(FsInfo.camera[4])},
	{ENUM_FS_CAMERA6, &(FsInfo.camera[5])},
	{ENUM_FS_ALTITUDE, &(FsInfo.altitude)},
	{ENUM_FS_GNSS_TAKION, &(FsInfo.gnss_takion)},
	{ENUM_FS_INS, &(FsInfo.ins)},
	{ENUM_FS_IMU, &(FsInfo.imu)},
	{ENUM_FS_WIFI, &(FsInfo.wifi)},
	{ENUM_FS_ATM_PRESSURE, &(FsInfo.atm_pressure)},
	{ENUM_FS_ECU_JETSON1, &(FsInfo.ecu_jetson[0])},
	{ENUM_FS_ECU_JETSON2, &(FsInfo.ecu_jetson[1])},
	{ENUM_FS_ECU_JETSON3, &(FsInfo.ecu_jetson[2])},
	{ENUM_FS_MAG, &(FsInfo.mag)},
	{ENUM_FS_ECU, &(FsInfo.ecu)},
};									


/* ************************************************************************** */
/* global 変数宣言                                                            */
/* ************************************************************************** */


/* ************************************************************************** */
/* プロトタイプ宣言(内部関数)                                                 */
/* ************************************************************************** */
static int32_t com_fs_GetID(uint32_t aErrcode);

/* ************************************************************************** */
/* 関数定義                                                                   */
/* ************************************************************************** */

/* ************************************************************************** */
/* 
 * 関数名   警告情報取得
 * 機能     警告情報を取得する．
 * 引数:    errcode：[i] 警告コード
 * 戻り値:  故障レベル
 * 作成日   2023/12/13 [0.0.1] 新規作成
 */
/* ************************************************************************** */
int32_t com_fs_getfail(uint32_t errcode)
{
    int32_t ret;
	int32_t tFsShmID;
	int32_t index;

    /* フェールセーフの共有メモリオープン */
	tFsShmID = com_shmem_open(DEF_FS_SHMEM_NAME, SHM_KIND_PLATFORM);
	if(tFsShmID == DEF_COM_SHMEM_FALSE)
    {
                dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_FS_SHMEM_NAME);
        pthread_exit(NULL);
    }

    com_shmem_read(tFsShmID, &FsInfo, sizeof(FsInfo));
    index = com_fs_GetID(errcode);
    ret = *FsState[index].data;
    
    /* 共有メモリクローズ */
    com_shmem_close(tFsShmID);

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
static int32_t com_fs_GetID(uint32_t aErrcode)
{
	int32_t ret = -1;

	for (int32_t cnt = 0; cnt < DEF_FS_ERR_MAX; cnt++)
	{
		if (aErrcode == FsState[cnt].errcode)
		{
			ret = cnt;
			break;
		}
		
	}
	return ret;
}

