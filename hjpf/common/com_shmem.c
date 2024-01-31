/*============================================================================*/
/*
 * @file    com_shmem.c
 * @brief   共有メモリ
 * @note    共有メモリ関連処理を行う。
 * @date    2023/11/14
 */
 /*============================================================================*/
 /*============================================================================*/
 /* include */
 /*============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/sem.h>
#include <errno.h>
#include "com_shmem.h"
#include "debug.h"
#include <string.h>
#include <glib.h>
#include <libgen.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <unistd.h>

/*============================================================================*/
/* global */
/*============================================================================*/
extern int	errno;
static int32_t sShmNum = 1;	/* 共有メモリの数 */
static pthread_mutex_t g_mutex;   
static memoryInfo saShmMng[DEF_COM_SHMEM_MAX];	/* 共有メモリ情報 */
//static char sShmEmpty[DEF_COM_SHMEM_PATH_MAX];

/*============================================================================*/
/* prototype */
/*============================================================================*/
static int32_t com_shmem_get_ID(char* aShmName);
static int32_t com_shmem_dump(int32_t aCnt);

/*============================================================================*/
/* const */
/*============================================================================*/


/*============================================================================*/
/*
 * @brief   共有メモリ設定ファイル読込
 * @note    共有メモリ設定ファイルを読み込む．
 *			設定ファイル有無，設定項目，サイズチェックを行う．
 * @param   引数  : 設定ファイル名
 * @return  戻り値:0：正常終了，-1：エラー
 * @date    2023/11/20 [0.0.1] プロセス管理種別を追加．
 */
 /*============================================================================*/
int32_t com_shmem_conf(char filename[])
{
	int32_t ret = DEF_COM_SHMEM_TRUE;	/* 戻り値変数 */
	GKeyFile* tShmKeyFile;
	GError* err = NULL;
	gchar** tGroupArray;
	gsize group_size;
	char* tDirName;
	struct stat tStatBuff;

	if (pthread_mutex_init(&g_mutex, NULL) != 0) {                                    
		dprintf(WARN, "pthread_mutex_init() error=%d\n", errno);
		return DEF_COM_SHMEM_FALSE;
	} 

	tShmKeyFile = g_key_file_new();	/* ファイルオブジェクト作成 */

	if (!g_key_file_load_from_file(tShmKeyFile, filename, 0, &err))	/* コンフィグファイルのオープン */
	{
		dprintf(ERROR, "load memory conf file failed. filename = %s\n", filename);
		ret = DEF_COM_SHMEM_FALSE;
	}
	else
	{
		tGroupArray = g_key_file_get_groups(tShmKeyFile, &group_size);	/* グループ数を取得 */
		sShmNum = (int32_t)group_size;	/* 共有メモリ数に変換 */

		for (int cnt = 0; cnt < sShmNum; cnt++)
		{
			strcpy(saShmMng[cnt].name, (char*)tGroupArray[cnt]);	/* 共有メモリ名を取得 */


			saShmMng[cnt].size = (int32_t)g_key_file_get_integer(tShmKeyFile, tGroupArray[cnt], "size", &err);	/* サイズを取得 */

			if ((0 >= saShmMng[cnt].size) || (NULL != err))
			{
				dprintf(ERROR, "Share Memory : %s , failed to parse memory size(Due to size=%d).\n", saShmMng[cnt].name, saShmMng[cnt].size);
				ret = DEF_COM_SHMEM_FALSE;
			}

			saShmMng[cnt].kind = (int32_t)g_key_file_get_integer(tShmKeyFile, tGroupArray[cnt], "kind", &err);	/* 種別を取得 */
			if (((SHM_KIND_PLATFORM != saShmMng[cnt].kind) && (SHM_KIND_USER != saShmMng[cnt].kind) && (SHM_KIND_PROC != saShmMng[cnt].kind)) || (NULL != err))
			{
				dprintf(ERROR, "failed to parse memory kind(No.%d).\n", cnt);
				ret = DEF_COM_SHMEM_FALSE;
			}

			strcpy(saShmMng[cnt].path, (char*)g_key_file_get_string(tShmKeyFile, tGroupArray[cnt], "path", &err));	/* 保存ファイル名を取得 */
			char tPathName[DEF_COM_SHMEM_PATH_MAX];
			strcpy(tPathName, saShmMng[cnt].path);
			tDirName = dirname(tPathName);
			if (saShmMng[cnt].path[0] != '\0')
			{
				if (!((stat(tDirName, &tStatBuff) == DEF_COM_SHMEM_TRUE) && S_ISDIR(tStatBuff.st_mode)))
				{
					dprintf(WARN, "Directory Name : %s, error. errno=%d\n", tDirName, errno);
					ret = DEF_COM_SHMEM_FALSE;
				}

				if (NULL != err)
				{
					dprintf(WARN, "failed to parse dump file name(No.%d). msg=%s\n", cnt, err->message);
				}
			}
			else
			{
				dprintf(INFO,"path(%d) is empty. \n", cnt);
			}
		}
		g_strfreev(tGroupArray);
	}

	g_key_file_free(tShmKeyFile);	/* ファイルの開放処理 */

	if (NULL != err)
	{
		g_error_free(err);
	}

	return ret;
}

/*============================================================================*/
/*
 * @brief   共有メモリ，セマフォを生成する
 * @note    設定数分以下を繰り返す
 *			共有メモリ生成
 *			共有メモリサイズを設定
 *			セマフォ生成
 * @param   引数  : なし
 * @return  戻り値：0：正常終了，-1：エラー
 * @date    2023/11/15 [0.0.1] 新規作成
 */
 /*============================================================================*/
int32_t com_shmem_init(void)
{
	int32_t ret = DEF_COM_SHMEM_TRUE;

	for (int32_t cnt = 0; cnt < sShmNum; cnt++)
	{
		saShmMng[cnt].shmfd = shm_open(saShmMng[cnt].name, O_RDWR | O_CREAT, DEF_COM_SHMEM_MODE);	/* 共有メモリ生成 */
		if (saShmMng[cnt].shmfd != DEF_COM_SHMEM_FALSE)
		{
			if (ftruncate(saShmMng[cnt].shmfd, saShmMng[cnt].size) == DEF_COM_SHMEM_FALSE)	/* 共有メモリサイズ設定 */
			{
				dprintf(ERROR, "Share Memory : %s , fail to set size. errno=%d\n", saShmMng[cnt].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
			close(saShmMng[cnt].shmfd);	/* ファイルを閉じる */
		}
		else
		{
			dprintf(ERROR, "Share MEmory : %s, fail to init share memory. errno=%d\n", saShmMng[cnt].name, errno);
			ret = DEF_COM_SHMEM_FALSE;
		}

		saShmMng[cnt].sem = sem_open(saShmMng[cnt].name, O_CREAT);	/* セマフォ生成 */
			if (saShmMng[cnt].sem == SEM_FAILED)
			{
				dprintf(WARN, "Semaphore : %s, fail to init semaphore. errno=%d\n", saShmMng[cnt].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
				break;
			}
			
		if (saShmMng[cnt].path[0] != '\0')
		{
			if (com_shmem_open(saShmMng[cnt].name, saShmMng[cnt].kind) != DEF_COM_SHMEM_FALSE)
			{
				memset(saShmMng[cnt].address, 0x0, saShmMng[cnt].size);	/*  */
				FILE* tpFile = fopen(saShmMng[cnt].path, "rb");
				if (tpFile != NULL)
				{
					if (sem_wait(saShmMng[cnt].sem) == DEF_COM_SHMEM_TRUE)	/* セマフォをロック */
					{
						fread(saShmMng[cnt].address, saShmMng[cnt].size, 1, tpFile);	/* ダンプファイルの読み込み */

						if (sem_post(saShmMng[cnt].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをアンロック */
						{
							dprintf(ERROR, "Semaphore : %s, fail to unlock semaphore. errno=%d.\n", saShmMng[cnt].name, errno);
							ret = DEF_COM_SHMEM_FALSE;
						}
					}
					else
					{
						dprintf(ERROR, "Semaphore : %s, fail to lock semaphore. errno=%d.\n", saShmMng[cnt].name, errno);
						ret = DEF_COM_SHMEM_FALSE;
					}
					fclose(tpFile);
				}
				if (com_shmem_close(cnt) == DEF_COM_SHMEM_FALSE)
				{
					dprintf(ERROR, "fail to close share memory or semaphore after reading dump file.\n");
					ret = DEF_COM_SHMEM_FALSE;
				}
			}
			else
			{
				dprintf(ERROR, "fail to open share memory or semaphore, and fail to read dump file.\n");
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
		
	}

	return ret;
}

/*============================================================================*/
/*
 * @brief   共有メモリ，セマフォをオープンする
 * @note    共有メモリ管理IDを取得
 *			共有メモリをオープン
 *			共有メモリをマッピング
 *			セマフォをオープン
 * @param   引数  : 共有メモリ名
 *					種別(1：プラットフォーム，2：ユーザプロセス)
 * @return  戻り値：0以上：共有メモリID，-1：エラー
 * @date    2023/11/15 [0.0.1] 新規作成
 */
 /*============================================================================*/
int32_t com_shmem_open(char* aShmName, enum shm_kind aKind)
{
	int32_t ret = DEF_COM_SHMEM_TRUE;
	int32_t tShmID;

	pthread_mutex_lock(&g_mutex);

	tShmID = com_shmem_get_ID(aShmName);	/* 共有メモリ管理IDを取得 */
	if (tShmID == DEF_COM_SHMEM_FALSE) {
		// dprintf(ERROR, "Share Memory : com_shmem_get_ID(%s) error\n", aShmName);
		pthread_mutex_unlock(&g_mutex);
		return DEF_COM_SHMEM_FALSE;
	}

	saShmMng[tShmID].counter++;
	if (saShmMng[tShmID].counter > 1) {
		pthread_mutex_unlock(&g_mutex);
		return tShmID;
	}

	saShmMng[tShmID].shmfd = shm_open(saShmMng[tShmID].name, O_RDWR | O_CREAT, DEF_COM_SHMEM_MODE);	/* 共有メモリをオープン */
	saShmMng[tShmID].current = aKind;	/* カレント種別を更新 */

	if (saShmMng[tShmID].shmfd != DEF_COM_SHMEM_FALSE)
	{
		saShmMng[tShmID].address = mmap(NULL, saShmMng[tShmID].size, PROT_READ | PROT_WRITE, MAP_SHARED,
			saShmMng[tShmID].shmfd, DEF_COM_SHMEM_OFFSET);	/* 共有メモリをマッピング */

		if (saShmMng[tShmID].address == MAP_FAILED)
		{
			dprintf(ERROR, "Share Memory : %s, fail to mapping. errno=%d\n", saShmMng[tShmID].name, errno);
			ret = DEF_COM_SHMEM_FALSE;
		}

		saShmMng[tShmID].sem = sem_open(saShmMng[tShmID].name, O_CREAT);	/* セマフォをオープン */

		if (saShmMng[tShmID].sem != SEM_FAILED)
		{
			if (ret != DEF_COM_SHMEM_FALSE)
			{
				ret = tShmID;
			}
		}
		else
		{
			dprintf(ERROR, "Semaphore : %s, fail to open semaphore. errno=%d\n", saShmMng[tShmID].name, errno);
			ret = DEF_COM_SHMEM_FALSE;
		}

	}
	else
	{
		dprintf(ERROR, "Share Memory : %s, fail to open share memory. errno=%d\n", saShmMng[tShmID].name, errno);
		ret = saShmMng[tShmID].shmfd;
		//ret = DEF_COM_SHMEM_FALSE;
		
	}

	pthread_mutex_unlock(&g_mutex);
	return ret;
}

/*============================================================================*/
/*
 * @brief   共有メモリ，セマフォをクローズする
 * @note    共有メモリ管理IDを取得
 *			共有メモリをクローズ
 *			セマフォをクローズ
 * @param   引数  : 共有メモリID
 * @return  戻り値：0：正常終了，-1：エラー
 * @date    2023/11/15 [0.0.1] 新規作成
 */
 /*============================================================================*/
int32_t com_shmem_close(int32_t aShmID)
{
	int32_t		ret = DEF_COM_SHMEM_TRUE;

	pthread_mutex_lock(&g_mutex);
	if ((aShmID <= sShmNum) && (aShmID >= 0))	/* 引数のチェック */
	{	
		saShmMng[aShmID].counter--;
		if (saShmMng[aShmID].counter > 0) {
			pthread_mutex_unlock(&g_mutex);
			return ret;
		}
		if (saShmMng[aShmID].sem != SEM_FAILED)	/* セマフォがオープンされているかチェック */
		{
			if (sem_close(saShmMng[aShmID].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをクローズ */
			{
				dprintf(ERROR, "Semaphore : %s, fail to close semaphore. errno=%d\n", saShmMng[aShmID].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
		else
		{
			dprintf(INFO, "Semaphore : %s is already closed.\n", saShmMng[aShmID].name);
		}

		if (saShmMng[aShmID].shmfd != DEF_COM_SHMEM_FALSE)	/* 共有メモリがオープンされているかチェック */
		{
			if (munmap(saShmMng[aShmID].address, saShmMng[aShmID].size) != DEF_COM_SHMEM_TRUE)	/* 共有メモリをクローズ */
			{
				dprintf(ERROR, "Share Memory : %s, fail to close share memory. errno=%d\n", saShmMng[aShmID].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
		else
		{
			dprintf(INFO, "Share Memory : %s is already closed.\n", saShmMng[aShmID].name);
		}

		close(saShmMng[aShmID].shmfd);	/* ファイルをクローズ */

		/* 共有メモリとセマフォの初期化 */
		saShmMng[aShmID].sem = SEM_FAILED;
		saShmMng[aShmID].shmfd = DEF_COM_SHMEM_FALSE;
		saShmMng[aShmID].address = MAP_FAILED;
	}
	else
	{
		dprintf(WARN, "Invalid Argument (com_shmem_close(%d))\n", aShmID);
		ret = DEF_COM_SHMEM_FALSE;
	}

	pthread_mutex_unlock(&g_mutex);
	return ret;
}

/*============================================================================*/
/*
 * @brief   共有メモリ，セマフォを削除する
 * @note    設定数分以下を繰り返す
 *			・共有メモリがクローズされてない場合はクローズ
 *			・共有メモリを削除
 *			・セマフォがクローズされてない場合はクローズ
 *			・セマフォを削除
 * @param   引数  : なし
 * @return  戻り値:
 * @date    2023/11/15 [0.0.1] 新規作成
 */
 /*============================================================================*/
void com_shmem_destroy(void)
{
	for (int32_t cnt = 0; cnt < sShmNum; cnt++)
	{
		if (com_shmem_dump(cnt) == DEF_COM_SHMEM_FALSE)	/* グローバル変数のダンプ処理 */
		{
			dprintf(ERROR, "fail to dump.\n");
		}

		if (com_shmem_close(cnt) != DEF_COM_SHMEM_TRUE)
		{
			dprintf(ERROR, "fail to open share memory or semaphore.\n");
		}

		if (sem_unlink(saShmMng[cnt].name) != DEF_COM_SHMEM_TRUE)	/* セマフォを削除 */
		{
			dprintf(ERROR, "Semaphore : %s, fail to unlink semaphore. errno=%d.\n", saShmMng[cnt].name, errno);
		}

		if (shm_unlink(saShmMng[cnt].name) != DEF_COM_SHMEM_TRUE)	/* 共有メモリを削除 */
		{
			dprintf(ERROR, "Share Memory : %s, fail to unlink share memory. errno=%d.\n", saShmMng[cnt].name, errno);
		}
	}

}

/*============================================================================*/
/*
 * @brief   セマフォロックを行う．共有メモリからデータを読み込む．セマフォアンロックを行う．
 * @note    共有メモリ管理IDを取得
 *			セマフォロック
 *			共有メモリを読み込む
 *			セマフォアンロック
 * @param   引数  : 共有メモリID
 *					読み込むデータのアドレス
 *					読み込みサイズ
 * @return  戻り値：0：正常終了，-1：エラー
 * @date    2023/11/16 [0.0.1] 新規作成
 */
 /*============================================================================*/
int32_t com_shmem_read(int32_t aShmID, void* aData, int32_t aSize)
{
	int32_t		ret = DEF_COM_SHMEM_TRUE;
	if ((aData != NULL) && (aSize <= saShmMng[aShmID].size) && (aShmID <= sShmNum) && (aShmID >= 0))	/* 引数のチェック */
	{
#if 1
		if ((saShmMng[aShmID].address != MAP_FAILED) && (saShmMng[aShmID].sem != SEM_FAILED) &&
			(saShmMng[aShmID].shmfd != DEF_COM_SHMEM_FALSE))	/* 共有メモリ，セマフォのオープン確認 */
		{
			if (sem_wait(saShmMng[aShmID].sem) == DEF_COM_SHMEM_TRUE)	/* セマフォをロック */
			{
				memcpy(aData, saShmMng[aShmID].address, aSize);	/* 共有メモリを読み込む */

				if (sem_post(saShmMng[aShmID].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをアンロック */
				{
					dprintf(WARN, "Semaphore : %s, fail to unlock semaphore. errno=%d.\n", saShmMng[aShmID].name, errno);
					ret = DEF_COM_SHMEM_FALSE;
				}
			}
			else
			{
				dprintf(WARN, "Semaphore : %s, fail to lock semaphore. errno=%d.\n", saShmMng[aShmID].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
		else
		{
			dprintf(WARN, "Share Memory : %s is not opened.\n", saShmMng[aShmID].name);
			ret = DEF_COM_SHMEM_FALSE;
		}
#else
		if ((saShmMng[aShmID].address == MAP_FAILED) || (saShmMng[aShmID].sem == SEM_FAILED) ||
			(saShmMng[aShmID].shmfd == DEF_COM_SHMEM_FALSE))	/* 共有メモリ，セマフォのオープン確認 */
		{
			aShmID = com_shmem_open(saShmMng[aShmID].name, saShmMng[aShmID].current);
		}
		if (aShmID != DEF_COM_SHMEM_FALSE)
		{
			if (sem_wait(saShmMng[aShmID].sem) == DEF_COM_SHMEM_TRUE)	/* セマフォをロック */
			{
				memcpy(aData, saShmMng[aShmID].address, aSize);	/* 共有メモリを読み込む */

				if (sem_post(saShmMng[aShmID].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをアンロック */
				{
					dprintf(WARN, "セマフォ名：%s，アンロック失敗．errno=%d\n", saShmMng[aShmID].name, errno);
					ret = DEF_COM_SHMEM_FALSE;
				}
			}
			else
			{
				dprintf(WARN, "セマフォ名：%s，ロック失敗．errno=%d\n", saShmMng[aShmID].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
#endif
	}
	else
	{
		dprintf(ERROR, "Invalid Argument (com_shmem_read), arg1=%d, arg2=%s, arg3=%d.\n", aShmID, aData, aSize);
		ret = DEF_COM_SHMEM_FALSE;
	}

	return ret;

}

/*============================================================================*/
/*
 * @brief   セマフォロックを行う．共有メモリにデータを書き込む．セマフォアンロックを行う．
 * @note    共有メモリ管理IDを取得
 *			セマフォロック
 *			共有メモリ種別が同じなら共有メモリに書き込む
 *			セマフォアンロック
 * @param   引数  : 共有メモリID
 *					書き込むデータ(のアドレス)
 *					書き込みサイズ
 * @return  戻り値：0：正常終了，-1：エラー
 * @date    2023/11/16 [0.0.1] 新規作成
 */
 /*============================================================================*/
int32_t com_shmem_write(int32_t aShmID, void* aData, int32_t aSize)
{
	int32_t		ret = DEF_COM_SHMEM_TRUE;
	if ((aData != NULL) && (aSize <= saShmMng[aShmID].size) && (aShmID <= sShmNum) && (aShmID >= 0))	/* 引数のチェック */
	{
#if 1
		if ((saShmMng[aShmID].address != MAP_FAILED) && (saShmMng[aShmID].sem != SEM_FAILED) &&
			(saShmMng[aShmID].shmfd != DEF_COM_SHMEM_FALSE))	/* 共有メモリ，セマフォのオープン確認 */
		{
			if (sem_wait(saShmMng[aShmID].sem) == DEF_COM_SHMEM_TRUE)	/* セマフォをロック */
			{
				if (saShmMng[aShmID].kind == saShmMng[aShmID].current)	/* 種別のチェック */
				{
					memcpy(saShmMng[aShmID].address, aData, aSize);	/* 共有メモリに書き込む */
					if (sem_post(saShmMng[aShmID].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをアンロック */
					{
						dprintf(ERROR, "Semaphore : %s, fail to unlock semaphore. errno=%d.\n", saShmMng[aShmID].name, errno);
						ret = DEF_COM_SHMEM_FALSE;
					}
				}
				else
				{
					dprintf(ERROR, "Share Memory : %s, kind=%d, current=%d. Due to not match kind, not permit to write.\n", saShmMng[aShmID].name, saShmMng[aShmID].kind, saShmMng[aShmID].current);
					ret = DEF_COM_SHMEM_FALSE;
				}
			}
			else
			{
				dprintf(ERROR, "Semaphore : %s, fail to lock semaphore. errno=%d.\n", saShmMng[aShmID].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
		else
		{
			dprintf(ERROR, "Share Memory : %s is not opened.(address=%d, sem=%x, smfd=%d)\n", 
				saShmMng[aShmID].name, saShmMng[aShmID].address, saShmMng[aShmID].sem, saShmMng[aShmID].shmfd);
			ret = DEF_COM_SHMEM_FALSE;
		}
#else
		if ((saShmMng[aShmID].address == MAP_FAILED) || (saShmMng[aShmID].sem == SEM_FAILED) ||
			(saShmMng[aShmID].shmfd == DEF_COM_SHMEM_FALSE))	/* 共有メモリ，セマフォのオープン確認 */
		{
			aShmID = com_shmem_open(saShmMng[aShmID].name, saShmMng[aShmID].current);
		}
		if (aShmID != DEF_COM_SHMEM_FALSE)
		{
			if (sem_wait(saShmMng[aShmID].sem) == DEF_COM_SHMEM_TRUE)	/* セマフォをロック */
			{
				if (saShmMng[aShmID].kind == saShmMng[aShmID].current)	/* 種別のチェック */
				{
					memcpy(saShmMng[aShmID].address, aData, aSize);	/* 共有メモリに書き込む */
					if (sem_post(saShmMng[aShmID].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをアンロック */
					{
						dprintf(WARN, "セマフォ名：%s，アンロック失敗．errno=%d\n", saShmMng[aShmID].name, errno);
						ret = DEF_COM_SHMEM_FALSE;
					}
				}
				else
				{
					dprintf(WARN, "種別が違うため，書き込み不可\n");
					ret = DEF_COM_SHMEM_FALSE;
				}
			}
			else
			{
				dprintf(WARN, "セマフォ名：%s，ロック失敗．errno=%d\n", saShmMng[aShmID].name, errno);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}
#endif
	}
	else
	{
		dprintf(WARN, "Invalid Argument (com_shmem_write(%d(%s),0x%x,%d)\n", aShmID, saShmMng[aShmID].name, aData, aSize);
		ret = DEF_COM_SHMEM_FALSE;
	}

	return ret;

}

/*============================================================================*/
/*
 * @brief   共有メモリ管理IDを取得
 * @note    共有メモリ名に対する共有メモリ名管理IDを調べる
 * @param   引数  : 共有メモリ名
 * @return  戻り値：0以上：共有メモリID，-1：エラー
 * @date    2023/11/16 [0.0.1] 新規作成
 */
 /*============================================================================*/
static int32_t com_shmem_get_ID(char* aShmName)
{
	int32_t ret = DEF_COM_SHMEM_FALSE;

	for (int32_t cnt = 0; cnt < sShmNum; cnt++)
	{
		if (strcmp(aShmName, saShmMng[cnt].name) == 0)
		{
			ret = cnt;
			break;
		}
	}

	return ret;
}

/*============================================================================*/
/*
 * @brief   グローバル変数のダンプ処理
 * @note
 * @param   引数  : 共有メモリ管理ID
 * @return  戻り値：0：正常終了，-1：エラー
 * @date    2023/11/16 [0.0.1] 新規作成
 */
 /*============================================================================*/
static int32_t com_shmem_dump(int32_t aCnt)
{
	int32_t ret = DEF_COM_SHMEM_TRUE;
	if (saShmMng[aCnt].path[0] != '\0')
		{
			if (com_shmem_open(saShmMng[aCnt].name, saShmMng[aCnt].kind) != DEF_COM_SHMEM_FALSE)	/* 共有メモリ，セマフォのオープン */
			{
				//memset(saShmMng[aCnt].address, 0x0, saShmMng[aCnt].size);	/*  */
				FILE* tpFile = fopen(saShmMng[aCnt].path, "wb");
				if (tpFile != NULL)
				{
					if (sem_wait(saShmMng[aCnt].sem) == DEF_COM_SHMEM_TRUE)	/* セマフォをロック */
					{
						fwrite(saShmMng[aCnt].address, saShmMng[aCnt].size, 1, tpFile);	/* ダンプファイルの書き込み */

						if (sem_post(saShmMng[aCnt].sem) != DEF_COM_SHMEM_TRUE)	/* セマフォをアンロック */
						{
							dprintf(ERROR, "Semaphore : %s, fail to unlock semaphore. errno=%d\n", saShmMng[aCnt].name, errno);
							ret = DEF_COM_SHMEM_FALSE;
						}
					}
					else
					{
						dprintf(ERROR, "Semaphore : %s, fail to lock semaphore. errno=%d\n", saShmMng[aCnt].name, errno);
						ret = DEF_COM_SHMEM_FALSE;
					}
					fclose(tpFile);
				}
				
				if (com_shmem_close(aCnt) == DEF_COM_SHMEM_FALSE)
				{
					dprintf(ERROR, "fail to close share memory(%s) or semaphore after dumping.\n", saShmMng[aCnt].name);
					ret = DEF_COM_SHMEM_FALSE;
				}
				
			}
			else	/* 共有メモリ，セマフォのオープン失敗 */
			{
				dprintf(ERROR, "fail to open share memory(%s) or semaphore, and fail to read dump file.\n", saShmMng[aCnt].name);
				ret = DEF_COM_SHMEM_FALSE;
			}
		}

	return ret;
}
