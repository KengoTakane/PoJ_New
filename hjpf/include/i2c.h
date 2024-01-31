/*============================================================================*/
/*
 * @file    i2c.h
 * @brief   I2C management
 * @note    I2C management
 * @date    2023/11/30
 */
/*============================================================================*/
#ifndef __I2C_H
#define __I2C_H

/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>

/*============================================================================*/
/* define */
/*============================================================================*/
#define DEF_I2C_DEVICE	"/dev/i2c-7"
#define DEF_HMC_SHMEM_NAME "/hmc6343"
#define DEF_BME_SHMEM_NAME "/bme680"
#define DEF_TIMER_KIND_I2C (3)						//timer id
#define DEF_MONIT_CYCLE_I2C (100)					//monitor period
/* HMC6343 */
#define DEF_HMC6343_ADDRESS				0x19		//Address of the hmc6343 itself
#define DEF_HMC6343_ACCELEROMETER_REG	0x40		//Accelerometer
#define DEF_HMC6343_MAGNETOMETER_REG	0x45		//Magnetometer
#define DEF_HMC6343_HEADING_REG			0x50		//Heading
#define DEF_HMC6343_TEMP_REG			0x55		//Temperature
/* BME680 */
#define DEF_BME680_ADDRESS				0x76		//Address of the bme680 itself
#define DEF_BME680_HUM_REG		    	0x72		//the bme680 status reg
#define DEF_BME680_MEAS_REG		    	0x74		//the bme680 status reg
#define DEF_BME680_CONF_REG		    	0x75		//the bme680 status reg
#define DEF_BME680_STATUS_REG			0x1D		//the bme680 status reg
#define DEF_BME680_DATALEN				15			//the bme680 datalen

/*============================================================================*/
/* typedef */
/*============================================================================*/
typedef struct _HMC6343{
	int32_t		Stat;
	uint32_t	Ax;
	uint32_t	Ay;
	uint32_t	Az;
	uint32_t	Mx;
	uint32_t	My;
	uint32_t	Mz;
	uint32_t	Head;
	uint32_t	Pitch;
	uint32_t	Roll;
	uint32_t	Temp;
} HMC6343;

typedef struct _BME680{
	int32_t		Stat;
    uint32_t	Press;
	int32_t		Temp;
	uint32_t	Hum;
	uint32_t	GasRes;
	uint32_t	GasRange;
} BME680;


/*============================================================================*/
/* enum */
/*============================================================================*/

/*============================================================================*/
/* struct */
/*============================================================================*/

/*============================================================================*/
/* func */
/*============================================================================*/

/*============================================================================*/
/* extern(val) */
/*============================================================================*/

/*============================================================================*/
/* extern(func) */
/*============================================================================*/
extern void* i2c_main(void* arg);

/*============================================================================*/
/* Macro */
/*============================================================================*/

#endif	/* __I2C_H */
