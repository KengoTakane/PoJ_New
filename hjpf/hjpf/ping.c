/* ************************************************************************** */
/* 
 * ファイル名           ping.c
 * コンポーネント名     PING処理
 * 説明                 PING処理
 * 会社名               パーソルエクセルHRパートナーズ（株）
 * 作成日               2023/12/08
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>

/* ************************************************************************** */
/* include(ユーザ定義ヘッダ)                                                  */
/* ************************************************************************** */
#include "com_timer.h"
#include "com_shmem.h"
#include "ping.h"
#include "hjpf.h"
#include "debug.h"
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


/* ************************************************************************** */
/* global 変数宣言                                                            */
/* ************************************************************************** */
extern int gComm_StopFlg;

/* ************************************************************************** */
/* プロトタイプ宣言(内部関数)                                                 */
/* ************************************************************************** */
static uint16_t ping_checksum(uint16_t* buf, size_t bufsz);
static int ping_open(char* hostname, struct sockaddr_in* addr);
static int32_t ping_sendwait(char* hostname, int timeout);


/* ************************************************************************** */
/* 関数定義                                                                   */
/* ************************************************************************** */

/* ************************************************************************** */
/* 
 * 関数名   PINGチェックサム計算
 * 機能     ICMPヘッダのチェックサムを計算する．
 * 引数:    buf：[i] バッファ
 *          bufsz：[i] バッファサイズ
 * 戻り値:  チェックサム値
 * 作成日   2023/12/08 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static uint16_t ping_checksum(uint16_t *buf, size_t bufsz)
{
	uint32_t	sum = 0;

	while (bufsz > 1) {
		sum += *buf;
		buf++;
		bufsz -= sizeof(uint16_t);
	}

	if (bufsz == 1) {
		sum += *(uint8_t *)buf;
	}

	sum = (sum & 0xffff) + (sum >> 16);
	sum += (sum >> 16);

	return ~sum & 0xffff;
}

/* ************************************************************************** */
/* 
 * 関数名   ソケットの生成
 * 機能     ホスト名からIPアドレスを取得して，ソケットを生成する．
 * 引数:    hostname：[i] ホスト名
 *          addr：[o] アドレス
 * 戻り値:  ソケットのファイルディスクリプタ
 * 作成日   2023/12/08 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int ping_open(char *hostname, struct sockaddr_in *addr)
{
	int	sock;
	struct addrinfo ainfo, *res;

	/* ホスト名からIPアドレスを取得する */
	memset(&ainfo, 0, sizeof(ainfo));
	ainfo.ai_socktype = SOCK_STREAM;
	ainfo.ai_family = AF_INET;
	if (getaddrinfo(hostname, NULL, &ainfo, &res) != 0) {
		dprintf(ERROR, "getaddrinfo(%s) error=%d\n", hostname, errno);
	}
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;

	freeaddrinfo(res);

	/* ソケット生成 */
	sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock < 0) {
		dprintf(ERROR, "socket(%s) error=%d\n", hostname, errno);
	}

	return sock;
}

/* ************************************************************************** */
/* 
 * 関数名   PING送受信
 * 機能     ICMPパケットを送信して，メッセージの受信を待つ．
 * 引数:    hostname：[i] 送信相手のホスト名
 *          timeout：[i] 受信を待つタイムアウト時間．
 * 戻り値:  OK, NG
 * 作成日   2023/12/08 [0.0.1] 新規作成
 */
/* ************************************************************************** */
static int32_t ping_sendwait(char *hostname, int timeout)
{
	dprintf(INFO, "ping_sendwait(%s, %d)\n", hostname, timeout);

	int32_t ret = DEF_PING_STATE_NG;
	struct sockaddr_in	addr;
	struct icmphdr	hdr;
	char	buf[1024];
	int	n;
	struct icmphdr *icmphdrptr;
	struct iphdr *iphdrptr;
	int	sock;
	fd_set	fds, readfds;
	struct timeval tv;

	/* ソケット生成 */
	sock = ping_open(hostname, &addr);
	if (sock < 0) {
		dprintf(ERROR, "ping_open(%s) error=%d\n", hostname, errno);
		pthread_exit(NULL);
		return ret;
	}

	memset(&hdr, 0, sizeof(hdr));

	/* ICMPヘッダ準備 */
	hdr.type = ICMP_ECHO;
	hdr.code = 0;
	hdr.checksum = 0;
	hdr.un.echo.id = 0;
	hdr.un.echo.sequence = 0;

	/* ICMPヘッダのチェックサムの計算 */
	hdr.checksum = ping_checksum((uint16_t*)&hdr, sizeof(hdr));

	/* ICMPヘッダだけのICMPパケットを送信 */
	n = sendto(sock, &hdr, sizeof(hdr), 0, (struct sockaddr*)&addr, sizeof(addr));
	if (n < 1) {
		dprintf(ERROR, "sendto() error=%d\n", errno);
		close(sock);
		return ret;
	}

	/* fd_setの初期化 */
	FD_ZERO(&readfds);
	/* select待ちソケットのセット */
	FD_SET(sock, &readfds);
	/* タイムアウトの設定 */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	/* 受信待ち */
	memcpy(&fds, &readfds, sizeof(fd_set));
	n = select(sock + 1, &fds, NULL, NULL, &tv);
	if (n == 0) {
		dprintf(ERROR, "recv(%s) timeout(%d) error=%d\n", hostname, timeout, errno);
		close(sock);
		return ret;
	} else if (n < 0) {
		dprintf(ERROR, "recv() select error=%d\n", errno);
		close(sock);
		return ret;
	}

	if (FD_ISSET(sock, &fds)) {
		memset(buf, 0, sizeof(buf));
		n = recv(sock, buf, sizeof(buf), 0);
		if (n < 1) {
			dprintf(ERROR, "recv() error=%d\n", errno);
			close(sock);
			return ret;
		}

		/* 受信データからIPヘッダ部へのポインタを取得 */
		iphdrptr = (struct iphdr*)buf;

		/* 受信データからICMPヘッダ部へのポインタを取得 */
		icmphdrptr = (struct icmphdr *)(buf + (iphdrptr->ihl * 4));

		/* ICMPヘッダからICMPの種類を特定します */
		if (icmphdrptr->type == ICMP_ECHOREPLY) {
			printf("[%s]received ICMP ECHO REPLY\n", hostname);
			ret = DEF_PING_STATE_OK;
		} else {
			switch(icmphdrptr->type) {
			case ICMP_UNREACH:
				dprintf(WARN, "[%s]received ICMP UNREACH\n", hostname);
				break;
			case ICMP_SOURCEQUENCH:
				dprintf(WARN, "[%s]received ICMP SOURCEQUENCH\n", hostname);
				break;
			case ICMP_REDIRECT:
				dprintf(WARN, "[%s]received ICMP REDIRECT\n", hostname);
				break;
			case ICMP_ECHO:
				dprintf(WARN, "[%s]received ICMP ECHO\n", hostname);
				break;
			case ICMP_TIMXCEED:
				dprintf(WARN, "[%s]received ICMP TIMXCEED\n", hostname);
				break;
			case ICMP_PARAMPROB:
				dprintf(WARN, "[%s]received ICMP PARAMPROB\n", hostname);
				break;
			default:
				dprintf(WARN, "[%s]received ICMP %d\n", hostname, icmphdrptr->type);
				break;
			}
		}
	}

	close(sock);

	return ret;
}

/* ************************************************************************** */
/* 
 * 関数名   PINGメイン処理
 * 機能     接続確認結果を，共有メモリへ書き込む．
 * 引数:    arg：[i] 引数
 * 戻り値:  なし
 * 作成日   2023/12/08 [0.0.1] 新規作成
 */
/* ************************************************************************** */
void* ping_main(void* arg)
{
	resPingInfo	*pingWiFi = (resPingInfo*)arg;
	int32_t tShmemID[pingWiFi->pingNum];
	STR_PING_INFO PingInfo;										/* PING接続情報 */

	com_timer_init(ENUM_TIMER_PING, pingWiFi->period);

	/* 共有メモリオープン */
	for (int cnt = 0; cnt < pingWiFi->pingNum; cnt++) 
	{
		tShmemID[cnt] = com_shmem_open(pingWiFi->shmname[cnt], SHM_KIND_PLATFORM);
		if(tShmemID[cnt] == DEF_COM_SHMEM_FALSE)
		{
			dprintf(WARN, "com_shmem_open error. name = %s\n", pingWiFi->shmname[cnt]);
			pthread_exit(NULL);
		}
	}

	while (!gComm_StopFlg) {
		for (int cnt = 0; cnt < pingWiFi->pingNum; cnt++) 
		{
			PingInfo.Stat = ping_sendwait(pingWiFi->hostname[cnt], pingWiFi->period);

			/* 共有メモリに書き込み */
        	com_shmem_write(tShmemID[cnt], &PingInfo, sizeof(PingInfo));
		}

		com_mtimer(ENUM_TIMER_PING);
	}

	/* 共有メモリクローズ */
	for (int cnt = 0; cnt < pingWiFi->pingNum; cnt++) 
	{
		com_shmem_close(tShmemID[cnt]);
	}

	pthread_exit(NULL);
}

