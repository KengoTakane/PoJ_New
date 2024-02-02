/*============================================================================*/
/*
 * @file    i2c.c
 * @brief   I2C取得
 * @note    I2C取得
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
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"
#include "resource.h"
#include "i2c.h"
#include "bme680.h"

/*============================================================================*/
/* global */
/*============================================================================*/
extern int gComm_StopFlg;
static struct bme680_dev gas_sensor;
static HMC6343 hmc;
static BME680 bme;

/*============================================================================*/
/* prototype */
/*============================================================================*/
static int i2c_open(char *devname);
static void i2c_close(int fd);
static int i2c_write(int fd, uint8_t dev_adr, uint8_t reg_addr, void *buf, uint32_t buf_length);
static int i2c_read(int fd, uint8_t dev_adr, uint8_t reg_addr, void *buf, uint32_t buf_length);
static int hmc_read(int fd);
static int	i2cfd;
	

/*============================================================================*/
/* const */
/*============================================================================*/

/*============================================================================*/
/* define */
/*============================================================================*/

/*============================================================================*/
/*
 * @brief   I2Cオープン
 * @note    I2Cをオープンする
 * @param   引数  : 
 * 		char *devname	デバイス名
 * @return  戻り値: ファイルディスクリプタ
 * @date    2023/12/01 [0.0.1] 新規作成
 */
/*============================================================================*/
static int i2c_open(char *devname)
{
	dprintf(INFO, "i2c_open(%s)\n", devname);

	int	fd;

	/* デバイスオープン */
	fd = open(devname, O_RDWR);
	if (fd < 0) {
		dprintf(ERROR, "i2c_open(%s) error=%d\n", devname, errno);
	}
	return fd;
}	

/*============================================================================*/
/*
 * @brief   I2Cクローズ
 * @note    I2Cをクローズする
 * @param   引数  :
 * 		int	fd	ファイルディスクリプタ
 * @return  戻り値: なし
 * @date    2023/12/01 [0.0.1] 新規作成
 */
/*============================================================================*/
static void i2c_close(int fd)
{
	/* デバイスクローズ */
	dprintf(INFO, "i2c_close(%d)\n", fd);
	close(fd);
}	

/*============================================================================*/
/*
 * @brief   I2C書き込み
 * @note    I2Cを書き込む
 * @param   引数  : 
 *				int			fd			ファイルディスクリプタ
 *				uint8_t		dev_sdr		デバイス(スレーブ)アドレス
 *				uint8_t		reg_sdr		レジスタアドレス
 *				void 		*buf		書き込むデータの格納場所を指すポインタ.
 *				uint32_t 	buf_length	書き込むデータの長さ.
 * @return  戻り値: 
 * @date    2023/12/01 [0.0.1] 新規作成
 */
/*============================================================================*/
static int i2c_write(int fd, uint8_t dev_adr, uint8_t reg_addr, void *buf, uint32_t buf_length)
{
	struct i2c_msg	msg;
	struct i2c_rdwr_ioctl_data	packets;
	unsigned char	buffer[1024];
	int32_t	ret;
	
	buffer[0] = reg_addr;	/* 1バイト目にレジスタアドレスをセット. */
	memcpy(&buffer[1], buf, buf_length);
	/* メッセージ作成 */
	msg.addr = dev_adr;
	msg.flags = 0;
	msg.len = buf_length + 1;	/* control byte + data bytes */
	msg.buf = buffer;
	
	/* 送信 */
	packets.msgs = &msg;
	packets.nmsgs = 1;
	ret = ioctl(fd, I2C_RDWR, &packets);
	if (ret < 0) {
		dprintf(ERROR, "i2c_write(%d) error=%d\n", fd, errno);
	}
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   I2C読み込み
 * @note    I2Cを読み込む
 *				int			fd			ファイルディスクリプタ
 *				uint8_t		dev_sdr		デバイス(スレーブ)アドレス
 *				uint8_t		reg_sdr		レジスタアドレス(読み込みたいデータの格納先)
 *				void 		*buf		読み込みデータ
 *				uint32_t 	buf_length	読み込みサイズ[bytes]
 * @return  戻り値: 
 * @date    2023/12/01 [0.0.1] 新規作成
 */
/*============================================================================*/
static int i2c_read(int fd, uint8_t dev_adr, uint8_t reg_addr, void *buf, uint32_t buf_length)
{
	struct i2c_msg	msg[2];	/* 1つ目にレジスタアドレスをセット，2つ目に読み込むサイズとデータの格納場所を指定 */
	struct i2c_rdwr_ioctl_data	packets;
	int32_t	ret;
	
	/* メッセージ作成 */
	msg[0].addr = dev_adr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;
	msg[1].addr = dev_adr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = buf_length;
	msg[1].buf = buf;
	
	/* 送信 */
	packets.msgs = msg;
	packets.nmsgs = 2;
	ret = ioctl(fd, I2C_RDWR, &packets);
	if (ret < 0) {
		dprintf(ERROR, "i2c_read(%d) error=%d\n", fd, errno);
	}
	
	return ret;
}

/*============================================================================*/
/*
 * @brief   HMC6343読み込み
 * @note    HMC6343情報を読み込む
 * @param   引数  : int	fd I2Cのファイルディスクリプタ
 * @return  戻り値: void*
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
static int hmc_read(int fd)
{
	int	ret;
	char	buf[6];

	if(fd == -1)
	{
		return DEF_RET_NG;
	}
	
	/* 加速度読み込み */
	ret = i2c_read(fd, DEF_HMC6343_ADDRESS, DEF_HMC6343_ACCELEROMETER_REG, buf, sizeof(buf));
	if (ret < 0) {
		dprintf(ERROR, "i2c_read(%d) error=%d\n", fd, errno);
		return ret;
	}
	hmc.Ax = (uint32_t)((buf[0] << 8) | buf[1]);
	hmc.Ay = (uint32_t)((buf[2] << 8) | buf[3]);
	hmc.Az = (uint32_t)((buf[4] << 8) | buf[5]);

	/* 磁力計読み込み */
	ret = i2c_read(fd, DEF_HMC6343_ADDRESS, DEF_HMC6343_MAGNETOMETER_REG, buf, sizeof(buf));
	if (ret < 0) {
		dprintf(ERROR, "i2c_read(%d) error=%d\n", fd, errno);
		return ret;
	}
	hmc.Mx = (uint32_t)((buf[0] << 8) | buf[1]);
	hmc.My = (uint32_t)((buf[2] << 8) | buf[3]);
	hmc.Mz = (uint32_t)((buf[4] << 8) | buf[5]);
	
	/* 方向読み込み */
	ret = i2c_read(fd, DEF_HMC6343_ADDRESS, DEF_HMC6343_HEADING_REG, buf, sizeof(buf));
	if (ret < 0) {
		dprintf(ERROR, "i2c_read(%d) error=%d\n", fd, errno);
		return ret;
	}
	hmc.Head = (uint32_t)((buf[0] << 8) | buf[1]);
	hmc.Pitch = (uint32_t)((buf[2] << 8) | buf[3]);
	hmc.Roll = (uint32_t)((buf[4] << 8) | buf[5]);

	/* 温度読み込み */
	ret = i2c_read(fd, DEF_HMC6343_ADDRESS, DEF_HMC6343_TEMP_REG, buf, sizeof(buf));
	if (ret < 0) {
		dprintf(ERROR, "i2c_read(%d) error=%d\n", fd, errno);
		return ret;
	}
	hmc.Temp = (uint32_t)((buf[4] << 8) | buf[5]);

//printf("%d, %d, %d\n", hmc.Ax, hmc.Ay, hmc.Az);

	return ret;
}

/*============================================================================*/
/*
 * @brief   ディレイ関数
 * @note    指定した時間[ms]ディレイさせる．
 * @param   引数  : uint32_t period ディレイさせる時間[ms]
 * @return  戻り値: int8_t 0:成功，-1:失敗
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
void user_delay_ms(uint32_t period)
{
	usleep(period * 1000);
}

/*============================================================================*/
/*
 * @brief   バス読込(ユーザー側)
 * @note    ユーザー側がI2C通信によるバス内のデータを読み込む
 * @param   引数  : 
 * @return  戻り値: int8_t 0:成功，-1:失敗
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
	int	ret;
	
	/* センサデータ読み込み */
	ret = i2c_read(i2cfd, dev_id, reg_addr, reg_data, len);
	if (ret < 0) {
		dprintf(ERROR, "i2c_read(%d) error=%d\n", i2cfd, errno);
		return ret;
	}
	return 0;
}

/*============================================================================*/
/*
 * @brief   バス書き込み(ユーザー側)
 * @note    ユーザー側がI2C通信によるバス内にデータを書き込む．
 * @param   引数  : 
 * @return  戻り値: int8_t 0:成功，-1:失敗
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
	int	ret;
	
	ret = i2c_write(i2cfd, dev_id, reg_addr, reg_data, len);
	if (ret < 0) {
		dprintf(ERROR, "i2c_write(%d) error=%d\n", i2cfd, errno);
	}
	return 0;
}

/*============================================================================*/
/*
 * @brief   BME初期化
 * @note    BME680デバイス情報を設定する．APIを用いて，デバイスのconfigrationを書き込む．
 * @param   引数  : 
 * @return  戻り値: int8_t 0:成功，-1:失敗
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
static int bme_init(int fd)
{
	int8_t rslt = BME680_OK;
	uint8_t set_required_settings;
	gas_sensor.dev_id = DEF_BME680_ADDRESS;
	gas_sensor.intf = BME680_I2C_INTF;
	gas_sensor.read = user_i2c_read;
	gas_sensor.write = user_i2c_write;
	gas_sensor.delay_ms = user_delay_ms;

	gas_sensor.amb_temp = 25;
	rslt = bme680_init(&gas_sensor);

	/* Set the temperature, pressure and humidity settings. os = over sampling   */
	gas_sensor.tph_sett.os_hum = BME680_OS_2X;
	gas_sensor.tph_sett.os_pres = BME680_OS_4X;
	gas_sensor.tph_sett.os_temp = BME680_OS_8X;
	gas_sensor.tph_sett.filter = BME680_FILTER_SIZE_3;
	/* Set the remaining gas sensor settings and link the heating profile */
	gas_sensor.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS;
	/* Create a ramp heat waveform in 3 steps */
	gas_sensor.gas_sett.heatr_temp = 320; /* degree Celsius */
	gas_sensor.gas_sett.heatr_dur = 150; /* milliseconds */

	/* Select the power mode */
	/* Must be set before writing the sensor configuration */
	gas_sensor.power_mode = BME680_FORCED_MODE; 

	/* Set the required sensor settings needed */
	set_required_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL | BME680_GAS_SENSOR_SEL;

	/* Set the desired sensor configuration */
	rslt = bme680_set_sensor_settings(set_required_settings,&gas_sensor);

	/* Set the power mode */
	rslt = bme680_set_sensor_mode(&gas_sensor);

	return rslt;
}

/*============================================================================*/
/*
 * @brief   BME680読み込み
 * @note    BME680情報を読み込む
 * @param   引数  : void*
 * @return  戻り値: void*
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
static int bme_read(int fd)
{
	int	ret;
	struct bme680_field_data data;

	ret = bme680_get_sensor_data(&data, &gas_sensor);
	bme.Press = data.pressure;
	bme.Temp = data.temperature;
	bme.Hum = data.humidity;
	bme.GasRes = data.gas_resistance;

	if (gas_sensor.power_mode == BME680_FORCED_MODE) {
		ret = bme680_set_sensor_mode(&gas_sensor);
	}

	return ret;
}

/*============================================================================*/
/*
 * @brief   I2Cメイン処理
 * @note    I2Cメイン処理
 * @param   引数  : void*
 * @return  戻り値: void*
 * @date    2023/11/30 [0.0.1] 新規作成
 */
/*============================================================================*/
void* i2c_main(void* arg)
{
	uint16_t meas_period;
	int id_hmc, id_bme;
	int ret = 0;
	resI2CInfo *i2cBME_HMC = (resI2CInfo*)arg;
	int timeout_cnt = 0;

	com_timer_init(ENUM_TIMER_I2C, i2cBME_HMC->period);

	//HMC(磁気センサ)共有メモリオープン
	id_hmc = com_shmem_open(DEF_HMC_SHMEM_NAME, SHM_KIND_PLATFORM);
	if(id_hmc == DEF_COM_SHMEM_FALSE){
		dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_HMC_SHMEM_NAME);
		pthread_exit(NULL);
	}

	//BME(大気圧計)共有メモリオープン
	id_bme = com_shmem_open(DEF_BME_SHMEM_NAME, SHM_KIND_PLATFORM);
	if(id_bme == DEF_COM_SHMEM_FALSE){
		dprintf(WARN, "com_shmem_open error. name = %s\n", DEF_BME_SHMEM_NAME);
		pthread_exit(NULL);
	}

	i2cfd = i2c_open(i2cBME_HMC->devname);
	if(i2cfd != -1)
	{
		bme_init(i2cfd);
		bme680_get_profile_dur(&meas_period, &gas_sensor);
	}

	hmc.Stat = 0;
	bme.Stat = 0;
	com_shmem_write(id_hmc, &hmc, sizeof(hmc));
	com_shmem_write(id_bme, &bme, sizeof(bme));

	while (!gComm_StopFlg) {
		//user_delay_ms(meas_period);
		//スリープ
		com_mtimer(ENUM_TIMER_I2C);
		timeout_cnt += i2cBME_HMC->period;
		
		// HMC
		ret = hmc_read(i2cfd);
		if(ret >= 0){
			timeout_cnt = 0;
			hmc.Stat = 0;
			com_shmem_write(id_hmc, &hmc, sizeof(hmc));
		}
		else
		{
			i2c_close(i2cfd);
			i2cfd = i2c_open(i2cBME_HMC->devname);
			if(i2cfd != -1)
			{
				bme_init(i2cfd);
				bme680_get_profile_dur(&meas_period, &gas_sensor);
			}

			if(timeout_cnt > i2cBME_HMC->timeout)
			{
				hmc.Stat = 1;
				com_shmem_write(id_hmc, &hmc, sizeof(hmc));
			}
		}

		//BME
		ret = bme_read(i2cfd);
		if(ret == 0){
			timeout_cnt = 0;
			hmc.Stat = 0;
			com_shmem_write(id_bme, &bme, sizeof(bme));
		}
		else
		{
			i2c_close(i2cfd);
			i2cfd = i2c_open(i2cBME_HMC->devname);
			if (i2cfd != -1) {
				bme_init(i2cfd);
				bme680_get_profile_dur(&meas_period, &gas_sensor);
			}

			if(timeout_cnt > i2cBME_HMC->timeout)
			{
				bme.Stat = 1;
				com_shmem_write(id_bme, &bme, sizeof(bme));
			}
		}
	}
	
	com_shmem_close(id_hmc);
	com_shmem_close(id_bme);

	i2c_close(i2cfd);
    pthread_exit(NULL);
}
