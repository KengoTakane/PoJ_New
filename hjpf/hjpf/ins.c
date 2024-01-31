/* ************************************************************************** */
/* 
 * ファイル名           ins.c
 * コンポーネント名     INSシリアル通信
 * 説明                 INSデータのシリアル受信を行う．
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/04
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
#include "ins.h"
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
static STR_INS_INFO InsInfo;                                   /* 受信したINSデータ */
//static char saInsSaveFrame[DEF_INS_SAVE_SIZE];					/* セーブフレームデータ */
static uint32_t sInsSaveLen = 0;								/* セーブフレームのサイズ */


/* ************************************************************************** */
/* global 変数宣言                                                            */
/* ************************************************************************** */
extern int gComm_StopFlg;
extern pthread_mutex_t g_mutex;   


/* ************************************************************************** */
/* プロトタイプ宣言(内部関数)                                                 */
/* ************************************************************************** */
static int32_t ins_serial_recv(int32_t aFiledes, char* aSaveFrame);
static int32_t ins_serial_checksum(char* aBuf);


/* ************************************************************************** */
/* 関数定義                                                                   */
/* ************************************************************************** */


/* ************************************************************************** */
/* 
 * 関数名   シリアル信号受信
 * 機能     INSシリアル信号を受信する．
 * 引数:    aFiledes：[i] ファイルディスクリプタ
 * 戻り値:  日本語説明
 *          成功失敗を示す。
 *          OK：成功、NG：失敗
 * 作成日   2023/12/04 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t ins_serial_recv(int32_t aFiledes, char* aSaveFrame)
{
    dprintf(INFO, "ins_serial_recv(%d, %s)\n", aFiledes, aSaveFrame);

    int32_t tRet = DEF_INS_TRUE;
    char tFrame[DEF_INS_SAVE_SIZE];
    char tBuf[DEF_INS_FRAME_SIZE];  /* RS-232Cのフレームデータ */
    char* tpCR;                     /* CR値のアドレス */
    int32_t tPos = 0;
    int32_t tSpos = 0;
    int32_t tSize = 0;
    int32_t tIndexCR = 0;
    int32_t tIndexSTX = 0;

    if(aFiledes == -1)
    {
        dprintf(ERROR, "INS FileDescript=-1, error=%d\n", errno);
        return DEF_RET_NG;
    }

    /* フレームデータの初期化 */
    memset(tFrame,0,sizeof(tFrame));

	pthread_mutex_lock(&g_mutex);
    /* シリアル受信 */
    tSize = read(aFiledes, tFrame, DEF_INS_READ_SIZE);
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
        memcpy(&aSaveFrame[sInsSaveLen], tFrame, tSize);
    } else {
        tRet = DEF_INS_READ_NODATA;
    }

    /* サイズの更新 */
    tSize = sInsSaveLen + tSize;

    for ( tPos = 0; tPos < tSize; tPos = tSpos +1)
    {
        /* セーブフレームからCR値を探索 */
        tpCR = memchr(&aSaveFrame[tPos], '\r', tSize - tPos);

        if (tpCR == NULL)
        {
            tRet = DEF_INS_FALSE;
            break;
        }

        /* CRの'\r'であるか確認 */
        tIndexCR = tpCR - aSaveFrame;
        tIndexSTX = tIndexCR - 25;

        /* セーブフレームのサイズ更新 */
        sInsSaveLen = tSize - (tIndexCR + 1);

        if (aSaveFrame[tIndexSTX] == 0x02)  /* STX値であるかチェック */
        {
            /* バッファにコピー */
            memcpy(tBuf, &aSaveFrame[tIndexSTX], DEF_INS_FRAME_SIZE);


            /* チェックサムの確認 */
            if(ins_serial_checksum(tBuf) == DEF_INS_TRUE)
            {
                /* ステータスの読み込み */
                InsInfo.Stat = (tBuf[3]<<8) | tBuf[4];

                /* 姿勢角の読み込み */
                InsInfo.Roll = (tBuf[5]<<8) | tBuf[6];
                InsInfo.Pitch = (tBuf[7]<<8) | tBuf[8];
                InsInfo.Yaw = (tBuf[9]<<8) | tBuf[10];

                /* 加速度の読み込み */
                InsInfo.Accel_X = (tBuf[11]<<8) | tBuf[12];
                InsInfo.Accel_Y = (tBuf[13]<<8) | tBuf[14];
                InsInfo.Accel_Z = (tBuf[15]<<8) | tBuf[16];

                /* 角速度の読み込み */
                InsInfo.Angl_X = (tBuf[17]<<8) | tBuf[18];
                InsInfo.Angl_Y = (tBuf[19]<<8) | tBuf[20];
                InsInfo.Angl_Z = (tBuf[21]<<8) | tBuf[22];

                tRet = DEF_INS_TRUE;
            }
            else
            {
                dprintf(WARN, "Invalid CheckSum.\n");
            }
            break;
        }

        /* インクリメント */
        tSpos = tpCR - aSaveFrame;
    }

    /* セーブフレームの更新 */
    if (sInsSaveLen > 2*DEF_INS_FRAME_SIZE)
    {
        sInsSaveLen = sInsSaveLen - DEF_INS_FRAME_SIZE;
    }
    memcpy(tFrame, &aSaveFrame[tSize - sInsSaveLen], sInsSaveLen);
    memcpy(aSaveFrame, tFrame, sInsSaveLen);

    return tRet;
}

/* ************************************************************************** */
/* 
 * 関数名   メイン処理
 * 機能     INSデータを受信し，共有メモリへ書き込む．
 * 引数:    arg：[i] 引数
 * 戻り値:  無し 
 * 作成日   2023/12/04 [0.0.1] 新規作成
 */
/* ************************************************************************** */
void* ins_serial_main(void* arg)
{
    int32_t tFiledes;
    int32_t tShmemID;
    char savefr[DEF_INS_SAVE_SIZE];
    int32_t tBaudrate = B115200;
	resUARTInfo *uartINS = (resUARTInfo*)arg;
    int ret;
    int timeout_cnt;

    /* 共有メモリオープン */
    tShmemID = com_shmem_open(DEF_INS_SHMEM_NAME, SHM_KIND_PLATFORM);
    if(tShmemID == DEF_COM_SHMEM_FALSE)
    {
        dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_INS_SHMEM_NAME);
        pthread_exit(NULL);
    }

    /* シリアル通信オープン */
    tFiledes = com_serial_open(uartINS->devname, tBaudrate);

    InsInfo.Stat = 0;
    com_shmem_write(tShmemID, &InsInfo, sizeof(InsInfo));
    
    /* INSデータ受信 */
    while(gComm_StopFlg == DEF_COMM_OFF)
    {
        usleep(10000);
        ret = ins_serial_recv(tFiledes, savefr);
        if (ret == DEF_INS_TRUE)
        {
            /* 共有メモリに書き込み */
            // printf("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d \n",
            //     InsInfo.Stat,
            //     InsInfo.Roll,
            //     InsInfo.Pitch,
            //     InsInfo.Yaw,
            //     InsInfo.Accel_X,
            //     InsInfo.Accel_Y,
            //     InsInfo.Accel_Z,
            //     InsInfo.Angl_X,
            //     InsInfo.Angl_Y,
            //     InsInfo.Angl_Z
            // );
            InsInfo.Stat = 0;
            timeout_cnt = 0;
            com_shmem_write(tShmemID, &InsInfo, sizeof(InsInfo));
        }
        else
        {
            usleep(1000);
            timeout_cnt++;

            if(ret == DEF_RET_NG)
            {
                com_serial_close(tFiledes);
                tFiledes = com_serial_open(uartINS->devname, tBaudrate);
            }
        }

        if(timeout_cnt > uartINS->timeout)
        {
            InsInfo.Stat = 1;
            com_shmem_write(tShmemID, &InsInfo, sizeof(InsInfo));
            com_serial_close(tFiledes);
            tFiledes = com_serial_open(uartINS->devname, tBaudrate);
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
static int32_t ins_serial_checksum(char* aBuf)
{
    int32_t tRet = DEF_INS_FALSE;
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
        tRet = DEF_INS_TRUE;
    }

    return tRet;
}

