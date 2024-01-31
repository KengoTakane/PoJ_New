/* ************************************************************************** */
/* 
 * ファイル名           altmt.h
 * コンポーネント名     高度計シリアル通信
 * 説明                 高度計データのシリアル受信を行う．
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/06
 * 作成者               高根
  */
/* ************************************************************************** */
#ifndef __ALTMT_H
#define __ALTMT_H
/* ************************************************************************** */
/* pragma定義                                                                 */
/* ************************************************************************** */


/* ************************************************************************** */
/* include(システムヘッダ)                                                    */
/* ************************************************************************** */
#include <stdint.h>

/* ************************************************************************** */
/* include(ユーザ定義ヘッダ)                                                  */
/* ************************************************************************** */


/* ************************************************************************** */
/* マクロ定義                                                                 */
/* ************************************************************************** */
#define DEF_ALTMT_FRAME_SIZE	(16)							/* UARTのフレームサイズ */
#define DEF_ALTMT_FALSE	(-1)									/* fault */
#define DEF_ALTMT_READ_NODATA (-2)
#define DEF_ALTMT_TRUE	(0)										/* true  */
#define DEF_ALTMT_READ_SIZE	(1024)								/* 読み込む受信データのサイズ */
#define DEF_ALTMT_SAVE_SIZE	(2049)								/* セーブフレームデータのサイズ */
#define DEF_ALTMT_SHMEM_NAME	"/altmt"						/* 共有メモリ名 */
#define DEF_ALTMT_VERSION	(0x6d)								/* 高度計のバージョン */
#define DEF_ALTMT_HEADER_H	(0x52)								/* ヘッダー(H) */
#define DEF_ALTMT_HEADER_L	(0x41)								/* ヘッダー(L) */


/* ************************************************************************** */
/* typedef 定義                                                               */
/* ************************************************************************** */


/* ************************************************************************** */
/* enum定義                                                                   */
/* ************************************************************************** */


/* ************************************************************************** */
/* struct/union定義                                                           */
/* ************************************************************************** */
/* 高度計データ */
typedef struct{
	int32_t				Stat;									/* 通信状況 */
	int32_t				Distance;								/* 距離 */
	int32_t				Radio;									/* 電波強度 */
	int32_t				Status;									/* ステータス */
}STR_ALTMT_INFO;




/* ************************************************************************** */
/* extern 変数宣言                                                            */
/* ************************************************************************** */


/* ************************************************************************** */
/* プロトタイプ宣言(公開関数)                                                 */
/* ************************************************************************** */
extern void* altmt_serial_main(void* arg);


#endif /* __ALTMT_H */
