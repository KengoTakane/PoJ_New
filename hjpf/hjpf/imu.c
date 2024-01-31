/* ************************************************************************** */
/* 
 * ファイル名           imu.c
 * コンポーネント名     IMUシリアル通信
 * 説明                 IMUデータのシリアル受信を行う．
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/05
 * 作成者               高根
  */
/* ************************************************************************** */
/* ************************************************************************** */
/* pragma定義                                                                 */
/* ************************************************************************** */


/* ************************************************************************** */
/* include(システムヘッダ)                                                    */
/* ************************************************************************** */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

/* ************************************************************************** */
/* include(ユーザ定義ヘッダ)                                                  */
/* ************************************************************************** */
#include "imu.h"
#include "debug.h"
#include "com_shmem.h"
#include "hjpf.h"
#include "resource.h"

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
static STR_IMU_INFO ImuInfo;								    /* 受信したIMUデータ */
static uint32_t sImuSaveLen = 0;								/* セーブフレームのサイズ */

/* ************************************************************************** */
/* global 変数宣言                                                            */
/* ************************************************************************** */
extern int gComm_StopFlg;
extern pthread_mutex_t g_mutex;   


/* ************************************************************************** */
/* プロトタイプ宣言(内部関数)                                                 */
/* ************************************************************************** */
static int32_t imu_serial_recv(int32_t aFiledes, char* aSaveFrame);
static int32_t imu_serial_checksum(char* aBuf);


/* ************************************************************************** */
/* 関数定義                                                                   */
/* ************************************************************************** */


/* ************************************************************************** */
/* 
 * 関数名   シリアル信号受信
 * 機能     IMUシリアル信号を受信する．
 * 引数:    aFiledes：[i] ファイルディスクリプタ
 *          aSaveFrame：[i/o] バッファに格納されてない残りのフレームデータ
 * 戻り値:  日本語説明
 *          成功失敗を示す。
 *          OK：成功、NG：失敗
 * 作成日   2023/12/05 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t imu_serial_recv(int32_t aFiledes, char* aSaveFrame)
{
    dprintf(INFO, "imu_serial_recv(%d, %s)\n", aFiledes, aSaveFrame);

    int32_t tRet = DEF_IMU_TRUE;
    char tFrame[DEF_IMU_SAVE_SIZE];
    char tBuf[DEF_IMU_FRAME_SIZE];  /* RS-422のフレームデータ */
    char* tpCR;                     /* エンドコードのアドレス */
    int32_t tPos = 0;
    int32_t tSpos = 0;
    int32_t tSize = 0;
    int32_t tIndexCR = 0;
    int32_t tIndexSTX = 0;

    if(aFiledes == -1)
    {
        dprintf(ERROR, "error=%d\n", errno);
        return DEF_RET_NG;
    }

    /* フレームデータの初期化 */
    memset(tFrame,0,sizeof(tFrame));

	pthread_mutex_lock(&g_mutex);
    /* シリアル受信 */
    tSize = read(aFiledes, tFrame, DEF_IMU_READ_SIZE);
	pthread_mutex_unlock(&g_mutex);

    if (tSize == -1)
    {
        /* エラーメッセージ出力 */
        dprintf(ERROR,"ins read error sock:%d\n", aFiledes);
		return DEF_RET_NG;
    }

    if(tSize > 0)
    {
        /* 読み込んだフレームを，セーブフレームの末尾に結合 */
        memcpy(&aSaveFrame[sImuSaveLen], tFrame, tSize);
    } else {
        tRet = DEF_IMU_READ_NODATA;
    }

    /* 読み込んだフレームを，セーブフレームの末尾に結合 */
    memcpy(&aSaveFrame[sImuSaveLen], tFrame, tSize);

    /* サイズの更新 */
    tSize = sImuSaveLen + tSize;

    for ( tPos = 0; tPos < tSize; tPos = tSpos +1)
    {
        /* セーブフレームからエンドコード値を探索 */
        tpCR = memchr(&aSaveFrame[tPos], '\r', tSize - tPos);

        if (tpCR == NULL)
        {
            tRet = DEF_IMU_FALSE;
            break;
        }

        if (!(tPos < 25))
        {
            /* エンドコードの'\r'であるか確認 */
            tIndexCR = tpCR - aSaveFrame;
            tIndexSTX = tIndexCR - 25;

            if (aSaveFrame[tIndexSTX] == 0x02)  /* スタートコードであるかチェック */
            {
                /* バッファにコピー */
                memcpy(tBuf, &aSaveFrame[tIndexSTX], DEF_IMU_FRAME_SIZE);

/*    
                printf("imu = ");
                for(int cnt = 0; cnt < DEF_IMU_FRAME_SIZE; cnt++)
                {
                    printf("%02x", tBuf[cnt]);
                }
                printf("\n");
*/

                /* セーブフレームのサイズ更新 */
                sImuSaveLen = tSize - (tIndexCR + 1);

                /* チェックサム判定 */
                if(imu_serial_checksum(tBuf) == DEF_IMU_TRUE)
                {
                    /* ステータスの読み込み */
                    ImuInfo.imu_Stat = (tBuf[3]<<8) | tBuf[4];

                    /* 姿勢角の読み込み */
                    ImuInfo.Roll = (tBuf[5]<<8) | tBuf[6];
                    ImuInfo.Pitch = (tBuf[7]<<8) | tBuf[8];
                    ImuInfo.Yaw = (tBuf[9]<<8) | tBuf[10];

                    /* 加速度の読み込み */
                    ImuInfo.Accel_X = (tBuf[11]<<8) | tBuf[12];
                    ImuInfo.Accel_Y = (tBuf[13]<<8) | tBuf[14];
                    ImuInfo.Accel_Z = (tBuf[15]<<8) | tBuf[16];

                    /* 角速度の読み込み */
                    ImuInfo.Angl_X = (tBuf[17]<<8) | tBuf[18];
                    ImuInfo.Angl_Y = (tBuf[19]<<8) | tBuf[20];
                    ImuInfo.Angl_Z = (tBuf[21]<<8) | tBuf[22];

                    tRet = DEF_IMU_TRUE;
                }
                else
                {
                    dprintf(WARN, "Invalid CheckSum.\n");
                    for(int cnt=0; cnt<DEF_IMU_FRAME_SIZE;cnt++)
                    {
                        fprintf(stderr, "%02x", tBuf[cnt]);
                    }
                    fprintf(stderr, "\n");
                }
            }
        }
        
        /* インクリメント */
        tSpos = tpCR - aSaveFrame;
    }

    /* セーブフレームの更新 */
    memcpy(tFrame, &aSaveFrame[tSize - sImuSaveLen], sImuSaveLen);
    memcpy(aSaveFrame, tFrame, sImuSaveLen);

    return tRet;
}

/* ************************************************************************** */
/* 
 * 関数名   メイン処理
 * 機能     IMUデータを受信し，共有メモリへ書き込む．
 * 引数:    arg：[i] 引数
 * 戻り値:  なし
 * 作成日   2023/12/05 [0.0.1] 新規作成
 */
/* ************************************************************************** */
void* imu_serial_main(void* arg)
{
    int32_t tFiledes;
    int32_t tShmemID;
    char savefr[DEF_IMU_SAVE_SIZE];
    int32_t tBaudrate = B1000000;
	resUARTInfo *uartIMU = (resUARTInfo*)arg;
    int ret;
    int timeout_cnt = 0;

    /* 共有メモリオープン */
    tShmemID = com_shmem_open(DEF_IMU_SHMEM_NAME, SHM_KIND_PLATFORM);
    if(tShmemID == DEF_COM_SHMEM_FALSE)
    {
        dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_IMU_SHMEM_NAME);
        pthread_exit(NULL);
    }

    /* シリアル通信オープン */
    tFiledes = com_serial_open(uartIMU->devname, tBaudrate);
    ImuInfo.Stat = 0;
    com_shmem_write(tShmemID, &ImuInfo, sizeof(ImuInfo));

    /* IMUデータ受信 */
    while(gComm_StopFlg == DEF_COMM_OFF)
    {
        ret = imu_serial_recv(tFiledes, savefr);

        if (ret == DEF_IMU_TRUE)
        {
            /* 共有メモリに書き込み */
            timeout_cnt = 0;
            ImuInfo.Stat = 0;
            com_shmem_write(tShmemID, &ImuInfo, sizeof(ImuInfo));
        }
        else
        {
            usleep(1000);
            timeout_cnt++;

            if(ret == DEF_RET_NG)
            {
                com_serial_close(tFiledes);
                tFiledes = com_serial_open(uartIMU->devname, tBaudrate);
            }
        }

        if(timeout_cnt > uartIMU->timeout)
        {
            ImuInfo.Stat = 1;
            com_shmem_write(tShmemID, &ImuInfo, sizeof(ImuInfo));
            com_serial_close(tFiledes);
            tFiledes = com_serial_open(uartIMU->devname, tBaudrate);
        }
    }
    
    
    /* 共有メモリクローズ */
    com_shmem_close(tShmemID);

    /* シリアル通信クローズ */
    com_serial_close(tFiledes);

    pthread_exit(NULL);
}

/* ************************************************************************** */
/* 
 * 関数名   チェックサム
 * 機能     チェックサムを判定する．
 * 引数:    aBuf：[i] 26バイトのフレームデータ
 * 戻り値:  成功失敗を示す。
 *          OK：成功、NG：失敗
 * 作成日   2023/12/04 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t imu_serial_checksum(char* aBuf)
{
    int32_t tRet = DEF_IMU_FALSE;
    int16_t tBCC = 0;
    int16_t tSum = 0;

    if (('0' <= aBuf[23]) && (aBuf[23] <= '9'))
	{
		tBCC = (aBuf[23] - '0') << 4;
	}
	else if (('A' <= aBuf[23]) && (aBuf[23] <= 'F')) 
	{
		tBCC = (aBuf[23] - 'A' + 0xa) << 4;
	}
	else
	{
		tBCC = aBuf[23] << 4;
	}

	if (('0' <= aBuf[24]) && (aBuf[24] <= '9'))
	{
		tBCC |= (aBuf[24] - '0');
	}
	else if (('A' <= aBuf[24]) && (aBuf[24] <= 'F')) {
		tBCC |= (aBuf[24] - 'A' + 0xa);
	}
	else
	{
		tBCC |= aBuf[24];
	}

	for (int cnt = 1; cnt < 23; cnt++)
	{
		tSum ^= aBuf[cnt];
	}

    if(tBCC == tSum)
    {
        tRet = DEF_IMU_TRUE;
    }

    return tRet;
}


