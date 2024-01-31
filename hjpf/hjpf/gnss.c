/*============================================================================*/
/*
 * @file    GNSS.c
 * @brief   GNSS取得
 * @note    GNSS取得
 * @date    2023/11/30
 */
/*============================================================================*/

/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"
#include "resource.h"
#include "gnss.h"

/*============================================================================*/
/* global */
/*============================================================================*/
static gnssStat GnssStat;
extern int gComm_StopFlg;
extern pthread_mutex_t g_mutex;   

/*============================================================================*/
/* prototype */
/*============================================================================*/
static int GNSSSerialRecv(int fd, char *savefr);
static int GNSSGGA(char* buf);
static int GNSSRMC(char* buf);
static int GNSSGSA(char* buf);
static int GNSSCheckSum(char* buf);

/*============================================================================*/
/* const */
/*============================================================================*/
/*============================================================================*/
/*
 * @brief   チェックサム判定
 * @note    チェックサム計算、判定
 * @param   引数  : センテンス、チェックサム
 * @return  戻り値: int
 * @date    2023/12/01 [0.0.1] 新規作成
 */
/*============================================================================*/
static int GNSSCheckSum(char* buf)
{
    char cs = 0x00;
    int sum_byte;
    char sum[DEF_BUF_SIZE];
    int ret = DEF_RET_OK;

    strtoks(buf, "*");
    strcpy(sum, strtoks(NULL, "\n"));

    buf++;

    while(*buf)
    {
        cs ^= *buf;
        buf++;
    }

    sum_byte = strtol(sum, NULL, 16);

    if(sum_byte != cs)
    {
        ret = DEF_RET_NG;
    }

    return ret;
}

/*============================================================================*/
/*
 * @brief   GGA処理
 * @note    GGA情報を構造体に格納
 * @param   引数  : センテンス
 * @return  戻り値:
 * @date    2019/12/23 [0.0.1] 新規作成
 */
/*============================================================================*/
static int GNSSGGA(char* buf)
{
    int ret = DEF_RET_OK;
    char tmp[DEF_BUF_SIZE];
    double latitude;
	double longitude;

    strcpy(tmp, buf);
    if(GNSSCheckSum(tmp))                                   //チェックサム確認
    {
        return DEF_RET_NG;
    }

    strtoks(buf,",");										//アドレスフィールド
    GnssStat.gga.time = atof(strtoks(NULL,","));				//UTC時刻

    latitude = atof(strtoks(NULL,","))/100;                 //緯度
    GnssStat.gga.latitude = (int)latitude + (latitude-(int)latitude)/0.6;
    
    if(strcmp(strtoks(NULL,","),"N")==0)                    //北緯 or 南緯
    {
        //北緯
        GnssStat.gga.latitude_sign = DEF_LATI_NORTH;
    }
    else
    {
        //南緯はマイナス値で保存
        GnssStat.gga.latitude *= -1;
        GnssStat.gga.latitude_sign = DEF_LATI_SOUTH;
    }

    longitude = atof(strtoks(NULL,","))/100;                //経度
    GnssStat.gga.longitude = (int)longitude + (longitude-(int)longitude)/0.6;
    
    if(strcmp(strtoks(NULL,","),"E")==0)                    //東経or西経
    { 
        //東経はプラス値のため処理なし
        GnssStat.gga.longitude_sign = DEF_LONG_EAST;
    }
    else
    {
        //西経はマイナス値で保存
        GnssStat.gga.longitude *= -1;
        GnssStat.gga.longitude_sign = DEF_LONG_WEST;
    }

    GnssStat.gga.mode_status = atoi(strtoks(NULL,","));			//測位モードステータス
    GnssStat.gga.satelite_num = atoi(strtoks(NULL,","));		//衛星捕捉数
    GnssStat.gga.hdop = atof(strtoks(NULL,","));				//水平精度低下率
    GnssStat.gga.height_sea = atof(strtoks(NULL,","));			//海抜高度
    strtoks(NULL,",");										//海抜高度単位
    GnssStat.gga.height_geoid = atof(strtoks(NULL,","));		//ジオイド高さ
    strtoks(NULL,",");										//ジオイド高さ単位
    strtoks(NULL,"\n");                                     //チェックサム

    return ret;
}


/*============================================================================*/
/*
 * @brief   RMC処理
 * @note    RMC情報を構造体に格納
 * @param   引数  : センテンス
 * @return  戻り値:
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
static int GNSSRMC(char* buf)
{
    int ret = DEF_RET_OK;
    char tmp[DEF_BUF_SIZE];
    double latitude;
	double longitude;

    strcpy(tmp, buf);
    if(GNSSCheckSum(tmp))                                   //チェックサム確認
    {
        return DEF_RET_NG;
    }

    strtoks(buf, ",");                                      //アドレスフィールド
    GnssStat.rmc.time = atof(strtoks(NULL, ","));               //UTC時刻
    strtoks(NULL, ",");                                     //測位状況ステータス
    
    latitude = atof(strtoks(NULL,","))/100;                 //緯度
    GnssStat.rmc.latitude = (int)latitude + (latitude-(int)latitude)/0.6;

    if(strcmp(strtoks(NULL,","),"N")==0)                    //北緯or南緯
    {
        //北緯はプラス値のため処理なし
        GnssStat.rmc.latitude_sign = DEF_LATI_NORTH;
    }
    else
    {
        //南緯はマイナス値で保存
        GnssStat.rmc.latitude *= -1;
        GnssStat.rmc.latitude_sign = DEF_LATI_SOUTH;
    }
    longitude = atof(strtoks(NULL,","))/100;
    GnssStat.rmc.longitude = (int)longitude + (longitude-(int)longitude)/0.6;

    if(strcmp(strtoks(NULL,","),"E")==0)                    //東経or西経
    {
        //東経はプラス値のため処理なし
        GnssStat.rmc.longitude_sign = DEF_LONG_EAST;
    }
    else
    {
        //西経はマイナス値で保存
        GnssStat.rmc.longitude *= -1;
        GnssStat.rmc.longitude_sign = DEF_LONG_WEST;
    }

    GnssStat.rmc.knots = atof(strtoks(NULL, ","));               //対地速度
    GnssStat.rmc.azimuth = atof(strtoks(NULL, ","));             //対地方位
    GnssStat.rmc.date = atoi(strtoks(NULL, ","));                //対地日付
    GnssStat.rmc.mag_dec = atof(strtoks(NULL, ","));             //磁気偏角
    if(strcmp(strtoks(NULL, ","), "E") == 0)		            //磁気偏角の方向
    {
    	GnssStat.rmc.mag_dec_dir = 0;
    }
    else
    {
    	GnssStat.rmc.mag_dec_dir = 1;
    }
    strcpy(GnssStat.rmc.mode_status, strtoks(NULL, ",*"));               //測位モードステータス
    strtoks(NULL,"\n");                                      //チェックサム

    return ret;
}

/*============================================================================*/
/*
 * @brief   GSA処理
 * @note    GSA情報を構造体に格納
 * @param   引数  : センテンス
 * @return  戻り値:
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
static int GNSSGSA(char* buf)
{
    int ret = DEF_RET_OK;
    char tmp[DEF_BUF_SIZE];

    strcpy(tmp, buf);
    if(GNSSCheckSum(tmp))                                   //チェックサム確認
    {
        return DEF_RET_NG;
    }

    strtoks(buf,",");										//アドレスフィールド
    strtoks(NULL,",");										//モード
    strtoks(NULL,",");										//特定タイプ
    strtoks(NULL,",");//衛星番号1
    strtoks(NULL,",");//衛星番号2
    strtoks(NULL,",");//衛星番号3
    strtoks(NULL,",");//衛星番号4
    strtoks(NULL,",");//衛星番号5
    strtoks(NULL,",");//衛星番号6
    strtoks(NULL,",");//衛星番号7
    strtoks(NULL,",");//衛星番号8
    strtoks(NULL,",");//衛星番号9
    strtoks(NULL,",");//衛星番号10
    strtoks(NULL,",");//衛星番号11
    strtoks(NULL,",");//衛星番号12
    GnssStat.gsa.pdop = atof(strtoks(NULL,","));			//位置精度低下率
    GnssStat.gsa.hdop = atof(strtoks(NULL,","));			//水平精度低下率
    strtoks(NULL,",");			                            //垂直精度低下率
    strtoks(NULL,"\n");                                     //チェックサム

    return ret;
}

/*============================================================================*/
/*
 * @brief   シリアル通信受信
 * @note    シグナル通信終了
 * @param   引数  : ファイルディスクリプタ、保存文字列
 * @return  戻り値:
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
static int GNSSSerialRecv(int fd, char *savefr)
{
    dprintf(INFO, "GNSSSerialRecv(%d, %s)\n", fd, savefr);

	char frame[DEF_FRAME_SIZE + 1];
	char buf[DEF_BUF_SIZE];
	char	*sptr;
	int pos = 0;
	int spos = 0;
	int	size = 0;
	int ret = 0;

    if(fd == -1)
    {
        dprintf(ERROR, "error=%d\n", errno);
        return DEF_RET_NG;
    }

    memset(frame,0,sizeof(frame));
    
	pthread_mutex_lock(&g_mutex);
	//シリアル受信
	size = read(fd, frame, DEF_READ_SIZE);
	pthread_mutex_unlock(&g_mutex);

	if( size == -1 )
    {
		dprintf(WARN, "gnss read error sock:%d\n", fd);
		//perror("read error");
		return DEF_RET_NG;
	}
	if( size == 0 )
    {
		//リードデータなし
		return DEF_GNSS_READ_NODATA;
	}

	//dprintf(WARN, "gnss fd:%d size:%d recv=[%s]\n", fd, size, frame);

#if DEF_GNSS_TEST
	printf("%s\n", frame);
#endif

	for(pos = 0; pos < size; pos = spos + 1)
    {
		sptr = strchr(&frame[pos], '\n');
		if (sptr == NULL) 
        {
			//改行なしの場合はセンテンスを保存して一旦終了
			strcat(savefr, &frame[pos]);
			//dprintf(WARN, "SAVEEND fd:%d savefr=[%s]\n", fd, savefr);
			break;
		}
		else
        {
			//保存センテンスがあれば取得
			if( savefr[0] != '\0' )
            {
				//保存済みセンテンスに読み取った文字列を結合
				strcat(savefr, &frame[pos]);

				//保存済みセンテンスを展開して初期化
				strcpy(frame, savefr);
				memset(savefr, 0, DEF_FRAME_SIZE);

				//改めて改行を検索
				sptr = strchr(&frame[pos], '\n');
				if(sptr == NULL)
                {
					//念のためチェック(基本的にはここは通らない)
					return -1;
				}

				//文字サイズ再取得
				size = strlen(frame);
			}
		}
		spos = sptr - frame;
		frame[spos] = '\0';
		strcpy(buf,&frame[pos]);
		//dprintf(WARN, "gnss fd:%d size=%d sentence=[%s]\n", fd, size, buf);

        //printf("%s\n", buf);

		if(strncmp(buf,"$GNGGA",6)==0)
        {
            ret = GNSSGGA(buf);
		}
        else if(strncmp(buf, "$GNRMC", 6) == 0)
        {
            ret = GNSSRMC(buf);
        }
		else if(strncmp(buf,"$GNGSA",6)==0)
        {
			ret = GNSSGSA(buf);
		}
		else
        {
			//異常データ
			//dprintf(WARN, "gnss err recv sock:%d [%s]\n", fd, buf);
		}
	}

	return ret;
}

/*============================================================================*/
/*
 * @brief   GNSSメイン処理
 * @note    GNSSメイン処理
 * @param   引数  : void*
 * @return  戻り値: void*
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
void* GNSSMain(void* arg){
    int fd;
    int ret;
    int id;
    char savefr[2048];
    int baudrate = B38400;
    int timeout_cnt = 0;
    resUARTInfo *uartGNSS = (resUARTInfo*)arg;

    //共有メモリオープン
    id = com_shmem_open(DEF_GNSS_SHMEM_NAME, SHM_KIND_PLATFORM);
    if(id == DEF_COM_SHMEM_FALSE)
    {
        com_serial_close(id);
        dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_GNSS_SHMEM_NAME);
        pthread_exit(NULL);
    }

    //シリアルオープン
    fd = com_serial_open(uartGNSS->devname, baudrate);

    GnssStat.Stat = 0;
    com_shmem_write(id, &GnssStat, sizeof(GnssStat));

    //データ受信
    while (gComm_StopFlg == DEF_COMM_OFF) 
    {
        ret = GNSSSerialRecv(fd, savefr);
        if(ret == DEF_RET_OK)
        {
            //共有メモリに書き込み
            GnssStat.Stat = 0;
            timeout_cnt = 0;
            com_shmem_write(id, &GnssStat, sizeof(GnssStat));
        } 
        else
        {
            usleep(1000);
            timeout_cnt++;

            if(ret == DEF_RET_NG)
            {
                com_serial_close(fd);
                fd = com_serial_open(uartGNSS->devname, baudrate);
            }
        }

        if(timeout_cnt > uartGNSS->timeout)
        {
            GnssStat.Stat = 1;
            com_shmem_write(id, &GnssStat, sizeof(GnssStat));
            com_serial_close(fd);
            fd = com_serial_open(uartGNSS->devname, baudrate);
        }
    }

    //共有メモリクローズ
    com_shmem_close(id);

    //シリアルポートクローズ
    com_serial_close(fd);
    
    pthread_exit(NULL);
}
