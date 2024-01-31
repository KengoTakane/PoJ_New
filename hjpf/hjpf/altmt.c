/* ************************************************************************** */
/* 
 * ファイル名           altmt.c
 * コンポーネント名     高度計シリアル通信
 * 説明                 高度計データのシリアル受信を行う．
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/06
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
#include "altmt.h"
#include "debug.h"
#include "com_shmem.h"
#include "hjpf.h"
#include "resource.h"

/* ************************************************************************** */
/* マクロ定義                                                                 */
/* ************************************************************************** */
#define DEF_CRC_INIT	0xFFFF
#define DEF_CRC_POLY	0x8408

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
static STR_ALTMT_INFO AltmtInfo;							/* 受信した高度計のデータ */
static uint32_t sAltmtSaveLen;									/* セーブフレームのサイズ */


/* ************************************************************************** */
/* global 変数宣言                                                            */
/* ************************************************************************** */
extern pthread_mutex_t g_mutex;   
extern int gComm_StopFlg;

/* ************************************************************************** */
/* プロトタイプ宣言(内部関数)                                                 */
/* ************************************************************************** */
static int32_t altmt_serial_recv(int32_t aFiledes, char* aSaveFrame);
static uint16_t crccheck(char* buf, int len);


/* ************************************************************************** */
/* 関数定義                                                                   */
/* ************************************************************************** */


/* ************************************************************************** */
/* 
 * 関数名   シリアル信号受信
 * 機能     シリアル信号を受信する．
 * 引数:    aFiledes：[i] ファイルディスクリプタ
 *          aSaveFrame：[i/o] バッファに格納されてない残りのフレームデータ
 * 戻り値:  日本語説明
 *          成功失敗を示す。
 *          OK：成功、NG：失敗
 * 作成日   2023/12/06 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t altmt_serial_recv(int32_t aFiledes, char* aSaveFrame)
{
    dprintf(INFO, "altmt_serial_recv(%d, %s)\n", aFiledes, aSaveFrame);

    int32_t tRet = DEF_ALTMT_TRUE;
    char tFrame[DEF_ALTMT_SAVE_SIZE];
    char tBuf[DEF_ALTMT_FRAME_SIZE];  /* UARTのフレームデータ */
    int32_t tPos = 0;
    int32_t tSize = 0;
    uint16_t crc;
    uint16_t crcdata;

    if(aFiledes == -1)
    {
        dprintf(ERROR, "error=%d\n", errno);
        return DEF_RET_NG;
    }

    /* フレームデータの初期化 */
    memset(tFrame,0,sizeof(tFrame));

	pthread_mutex_lock(&g_mutex);
    /* シリアル受信 */
    tSize = read(aFiledes, tFrame, DEF_ALTMT_READ_SIZE);
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
        memcpy(&aSaveFrame[sAltmtSaveLen], tFrame, tSize);
    } else {
        tRet = DEF_ALTMT_READ_NODATA;
    }

    /* 読み込んだフレームを，セーブフレームの末尾に結合 */
    memcpy(&aSaveFrame[sAltmtSaveLen], tFrame, tSize);

    /* サイズの更新 */
    tSize = sAltmtSaveLen + tSize;

    for ( tPos = 0; tPos < tSize; tPos++)
    {
        if (tSize - tPos >= 16)
        {
            /* ヘッダ値であるかチェック */
            if ((aSaveFrame[tPos] == DEF_ALTMT_HEADER_H) && (aSaveFrame[tPos + 1] == DEF_ALTMT_HEADER_L) && (aSaveFrame[tPos + 2] == DEF_ALTMT_VERSION))
            {
                /* バッファにコピー */
                memcpy(tBuf, &aSaveFrame[tPos], DEF_ALTMT_FRAME_SIZE);

/*
                printf("alt = ");
                for(int cnt = 0; cnt < DEF_ALTMT_FRAME_SIZE; cnt++)
                {
                    printf("%02x", tBuf[cnt]);
                }
                printf("\n");
*/
                

                /* セーブフレームのサイズ更新 */
                sAltmtSaveLen = tSize - 16;

                /* CRCチェック */
                memcpy(&crcdata, &tBuf[14], 2);
                crc = crccheck(tBuf, 14);
                if(crcdata == crc)
                {
                    /* ステータスの読み込み */
                    AltmtInfo.Status = (tBuf[12]<<8) | tBuf[13];

                    /* 距離の読み込み */
                    AltmtInfo.Distance = (tBuf[4]<<8) | tBuf[5];

                    /* 電波強度の読み込み */
                    AltmtInfo.Radio = (tBuf[10]<<8) | tBuf[11];

                    tRet = DEF_ALTMT_TRUE;
                }
                else
                {
                    dprintf(WARN, "Invalid CRC.\n");
                }

                /* インクリメント */
                tPos += (DEF_ALTMT_FRAME_SIZE - 1);
            }
        }
        
    }

    /* セーブフレームの更新 */
    memcpy(tFrame, &aSaveFrame[tSize - sAltmtSaveLen], sAltmtSaveLen);
    memcpy(aSaveFrame, tFrame, sAltmtSaveLen);

    return tRet;
}

/* ************************************************************************** */
/* 
 * 関数名   メイン処理
 * 機能     高度計のデータを受信し，共有メモリへ書き込む．
 * 引数:    arg：[i] 引数
 * 戻り値:  なし
 * 作成日   2023/12/06 [0.0.1] 新規作成
 */
/* ************************************************************************** */
void* altmt_serial_main(void* arg)
{
    int32_t tFiledes;
    int32_t tShmemID;
    char savefr[DEF_ALTMT_SAVE_SIZE];
    int32_t tBaudrate = B460800;
	resUARTInfo *uartALTMT = (resUARTInfo*)arg;
    int ret;
    int timeout_cnt = 0;

    /* 共有メモリオープン */
    tShmemID = com_shmem_open(DEF_ALTMT_SHMEM_NAME, SHM_KIND_PLATFORM);
    if(tShmemID == DEF_COM_SHMEM_FALSE)
    {
        dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_ALTMT_SHMEM_NAME);
        pthread_exit(NULL);
    }

    /* シリアル通信オープン */
    tFiledes = com_serial_open(uartALTMT->devname, tBaudrate);

    AltmtInfo.Stat = 0;
    com_shmem_write(tShmemID, &AltmtInfo, sizeof(AltmtInfo));
    
    /* IMUデータ受信 */
    while(gComm_StopFlg == DEF_COMM_OFF)
    {
        ret = altmt_serial_recv(tFiledes, savefr);
        if (ret == DEF_ALTMT_TRUE)
        {
            /* 共有メモリに書き込み */
            timeout_cnt = 0;
            AltmtInfo.Stat = 0;
            com_shmem_write(tShmemID, &AltmtInfo, sizeof(AltmtInfo));
        }
        else
        {
            usleep(1000);
            timeout_cnt++;

            if(ret == DEF_RET_NG)
            {
                com_serial_close(tFiledes);
                tFiledes = com_serial_open(uartALTMT->devname, tBaudrate);
            }
        }

        if(timeout_cnt > uartALTMT->timeout)
        {
            AltmtInfo.Stat = 1;
            com_shmem_write(tShmemID, &AltmtInfo, sizeof(AltmtInfo));
            com_serial_close(tFiledes);
            tFiledes = com_serial_open(uartALTMT->devname, tBaudrate);
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
 * 関数名   CRCチェック
 * 機能     
 * 引数:    arg：[i] 引数
 * 戻り値:  なし
 * 作成日   2023/12/07 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static uint16_t crccheck(char* buf, int len)
{
	uint16_t crc = DEF_CRC_INIT;
	uint16_t poly = DEF_CRC_POLY;
	for (int count = 0; count < len; count++) {
		crc = crc ^ buf[count];

		for (int j = 0; j < 8; j++) {
			if ((crc & 0x0001) == 0x0001) {
				crc = poly ^ (crc >> 1);
			}
			else {
				crc = crc >> 1;
			}
		}
	}

	return (crc & 0xFFFF) ^ 0xFFFF;
}