#include <stdio.h>
#include <unistd.h>
#include "../include/com_shmem.h"
#include "process.h"
#include "resource.h"
#include "gnss.h"
#include "ins.h"
#include "imu.h"
#include "altmt.h"
#include "i2c.h"
#include "ping.h"
#include "failsafe.h"
#include "com_fs.h"
#include "camera.h"
#include "mavlink.h"

//gcc -Include -c -o mem_read.o mem_read.c
//gcc -o mem_read mem_read.o ../common/com_shmem.o ../debug/debug.o -lpthread -lrt

int main(void)
{
	int id;
	
	com_shmem_conf("../hjpf/memory.conf");
//	com_shmem_init();

/* フェールセーフ */
#if 1
	failsafeInfo failInfo;
	id = com_shmem_open("/failsafeinfo", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) 
	{
		printf("/failsafeinfo com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, &failInfo, sizeof(failInfo));
	printf("Process = %d\n", failInfo.proc);
	for(int cnt = 0; cnt < 13; cnt++)
	{
		printf("CPULoad%d = %d\n", cnt, failInfo.cpu_load[cnt]);	
	}
	printf("MEMORY = %d\n", failInfo.mem);
	printf("DISK = %d\n", failInfo.disk);
	printf("THERM = %d\n", failInfo.cpu_therm);
	for(int cnt = 0; cnt < 6; cnt++)
	{
		printf("Camera%d = %d\n", cnt, failInfo.camera[cnt]);	
	}
	printf("altitude = %d\n", failInfo.altitude);
	printf("gnss = %d\n", failInfo.gnss_takion);
	printf("ins = %d\n", failInfo.ins);
	printf("imu = %d\n", failInfo.imu);
	printf("wifi = %d\n", failInfo.wifi);
	printf("bme = %d\n", failInfo.atm_pressure);
	for(int cnt = 0; cnt < 3; cnt++)
	{
		printf("jetson%d = %d\n", cnt, failInfo.ecu_jetson[cnt]);	
	}
	printf("hmc = %d\n", failInfo.mag);
	printf("cube pilot = %d\n", failInfo.ecu);
	com_shmem_close(id);
	printf("\n");
#endif

#if 1
	int value = 0;

	for (uint32_t i = 1; i <= 34; i++)
	{
		value = com_fs_getfail(i);
		printf("ErroCode = %d : fail level = %d\n", i, value);
	}
	printf("\n");
#endif

/*プロセス管理*/
#if 1 
	procStat ProcStat;
	id = com_shmem_open("/procstat", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/procstat com_shmem_open() error\n");
		return -1;
	}
	
	com_shmem_read(id, &ProcStat, sizeof(ProcStat));
	printf("proc num = %d\n", ProcStat.num);
	
	for(int i = 0; i < ProcStat.num; i++){
		printf("proc stat[%d] = %d\n", i, ProcStat.stat[i]);
	}

	for(int i = 0; i < ProcStat.num; i++){
		printf("proc cpu[%d] = %f\n", i, ProcStat.cpu[i]);
	}

	for(int i = 0; i < ProcStat.num; i++){
		printf("proc mem[%d] = %f\n", i, ProcStat.mem[i]);
	}
	com_shmem_close(id);
	printf("\n");
#endif

/* リソース管理 */
#if 1
	resourceStat ResStat;
	id = com_shmem_open("/resstat", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/resstat com_shmem_open() error\n");
		return -1;
	}
	
	com_shmem_read(id, &ResStat, sizeof(ResStat));
	for(int i = 0; i < 13; i++)
	{
		printf("cpu_load[%d] = %d\n", i, ResStat.cpu_load[i]);
	}
	printf("mem_load = %d\n", ResStat.mem_load);
	printf("disk_load = %d\n", ResStat.disk_load);
	printf("cpu_therm = %d\n", ResStat.cpu_therm);
	com_shmem_close(id);
	printf("\n");
#endif

/* GNSS */
#if 0
	gnssStat GNSS;
	id = com_shmem_open("/gnss", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/gnss com_shmem_open() error\n");
		return -1;
	}

	com_shmem_read(id, &GNSS, sizeof(GNSS));
	
	printf("GNSS.Stat = %d\n", GNSS.Stat);
	printf("GNSS.GGA.time = %f\n", GNSS.gga.time);
	printf("GNSS.GGA.latitude = %f\n", GNSS.gga.latitude);
	printf("GNSS.GGA.latitude_sign = %d\n", GNSS.gga.latitude_sign);
	printf("GNSS.GGA.longitude = %f\n", GNSS.gga.longitude);
	printf("GNSS.GGA.longitude_sign = %d\n", GNSS.gga.longitude_sign);
	printf("GNSS.GGA.mode_status = %d\n", GNSS.gga.mode_status);
	printf("GNSS.GGA.satelite = %d\n", GNSS.gga.satelite_num);
	printf("GNSS.GGA.hdop = %f\n", GNSS.gga.hdop);
	printf("GNSS.GGA.height_sea = %f\n", GNSS.gga.height_sea);
	printf("GNSS.GGA.height_geoid = %f\n", GNSS.gga.height_geoid);

	printf("GNSS.RMC.time = %f\n", GNSS.rmc.time);
	printf("GNSS.RMC.latitude = %f\n", GNSS.rmc.latitude);
	printf("GNSS.RMC.latitude_sign = %d\n", GNSS.rmc.latitude_sign);
	printf("GNSS.RMC.longitude = %f\n", GNSS.rmc.longitude);
	printf("GNSS.RMC.longitude_sign = %d\n", GNSS.rmc.longitude_sign);
	printf("GNSS.RMC.knots = %f\n", GNSS.rmc.knots);
	printf("GNSS.RMC.date = %d\n", GNSS.rmc.date);
	printf("GNSS.RMC.azimuth = %f\n", GNSS.rmc.azimuth);
	printf("GNSS.RMC.mag_dec = %f\n", GNSS.rmc.mag_dec);
	printf("GNSS.RMC.mag_dec_dir = %d\n", GNSS.rmc.mag_dec_dir);
	printf("GNSS.RMC.mode_status = %s\n", GNSS.rmc.mode_status);

	printf("GNSS.GSA.pdp = %f\n", GNSS.gsa.pdop);
	printf("GNSS.GSA.hdop = %f\n", GNSS.gsa.hdop);

	com_shmem_close(id);
	printf("\n");
#endif

/* INS */
#if 0
	STR_INS_INFO InsInfo;
	id = com_shmem_open("/ins", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/ins com_shmem_open() error\n");
		return -1;
	}

	com_shmem_read(id, &InsInfo, sizeof(InsInfo));
	printf("INS.Stat = %d\n", InsInfo.Stat);
	printf("INS.ins_Stat = %d\n", InsInfo.ins_Stat);
	printf("INS.Roll = %d\n", InsInfo.Roll);
	printf("INS.Pitch = %d\n", InsInfo.Pitch);
	printf("INS.Yaw = %d\n", InsInfo.Yaw);
	printf("INS.Accel_X = %d\n", InsInfo.Accel_X);
	printf("INS.Accel_Y = %d\n", InsInfo.Accel_Y);
	printf("INS.Accel_Z = %d\n", InsInfo.Accel_Z);
	printf("INS.Angl_X = %d\n", InsInfo.Angl_X);
	printf("INS.Angl_Y = %d\n", InsInfo.Angl_Y);
	printf("INS.Angl_Z = %d\n", InsInfo.Angl_Z);
	
	com_shmem_close(id);
	printf("\n");
#endif

/* IMU */
#if 0
	STR_IMU_INFO ImuInfo;
	id = com_shmem_open("/imu", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/ins com_shmem_open() error\n");
		return -1;
	}

	com_shmem_read(id, &ImuInfo, sizeof(ImuInfo));
	printf("IMU.Stat = %d\n", ImuInfo.Stat);
	printf("IMU.imu_Stat = %d\n", ImuInfo.imu_Stat);
	printf("IMU.Roll = %d\n", ImuInfo.Roll);
	printf("IMU.Pitch = %d\n", ImuInfo.Pitch);
	printf("IMU.Yaw = %d\n", ImuInfo.Yaw);
	printf("IMU.Accel_X = %d\n", ImuInfo.Accel_X);
	printf("IMU.Accel_Y = %d\n", ImuInfo.Accel_Y);
	printf("IMU.Accel_Z = %d\n", ImuInfo.Accel_Z);
	printf("IMU.Angl_X = %d\n", ImuInfo.Angl_X);
	printf("IMU.Angl_Y = %d\n", ImuInfo.Angl_Y);
	printf("IMU.Angl_Z = %d\n", ImuInfo.Angl_Z);
	
	com_shmem_close(id);
	printf("\n");
#endif

/* 高度計 */
#if 0
	STR_ALTMT_INFO AltmtInfo;
	id = com_shmem_open("/altmt", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/altmt com_shmem_open() error\n");
		return -1;
	}

	com_shmem_read(id, &AltmtInfo, sizeof(AltmtInfo));
	printf("AltmtInfo.Stat = %d\n", AltmtInfo.Stat);
	printf("AltmtInfo.Distance = %d\n", AltmtInfo.Distance);
	printf("AltmtInfo.Radio = %d\n", AltmtInfo.Radio);
	printf("AltmtInfo.Status = %d\n", AltmtInfo.Status);
	
	com_shmem_close(id);
	printf("\n");
#endif

/* hmc */
#if 0
	HMC6343 hmc;
	id = com_shmem_open("/hmc6343", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/hmc com_shmem_open() error\n");
		return -1;
	}

	com_shmem_read(id, &hmc, sizeof(hmc));
	printf("hmc.Stat = %d\n", hmc.Stat);
	printf("hmc.Ax = %d\n", hmc.Ax);
	printf("hmc.Ay = %d\n", hmc.Ay);
	printf("hmc.Az = %d\n", hmc.Az);
	printf("hmc.Mx = %d\n", hmc.Mx);
	printf("hmc.My = %d\n", hmc.My);
	printf("hmc.Mz = %d\n", hmc.Mz);
	printf("hmc.Head = %d\n", hmc.Head);
	printf("hmc.Pitch = %d\n", hmc.Pitch);
	printf("hmc.Roll = %d\n", hmc.Roll);
	printf("hmc.Temp = %d\n", hmc.Temp);
	com_shmem_close(id);
	printf("\n");
#endif

/* bme */
#if 0
	BME680 bme;
	id = com_shmem_open("/bme680", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/bme com_shmem_open() error\n");
		return -1;
	}

	com_shmem_read(id, &bme, sizeof(bme));
	printf("bme.Stat = %d\n", bme.Stat);
	printf("bme.Press = %d\n", bme.Press);
	printf("bme.Temp = %d\n", bme.Temp);
	printf("bme.Hum = %d\n", bme.Hum);
	printf("bme.GasRes = %d\n", bme.GasRes);
	com_shmem_close(id);
	printf("\n");
#endif


/* Wifi */
#if 0
	STR_PING_INFO PingInfo;
	id = com_shmem_open("/wifi", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/wifi com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, &PingInfo, sizeof(PingInfo));
	printf("Wifi connect Status = %d\n", PingInfo.Stat);

	com_shmem_close(id);
#endif

/* ECU */
#if 0
	STR_PING_INFO ECUInfo_0;
	id = com_shmem_open("/ecu0", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/ecu com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, &ECUInfo_0, sizeof(ECUInfo_0));
	printf("ECU0 connect Status = %d\n", ECUInfo_0.Stat);

	com_shmem_close(id);
#endif

/* ECU */
#if 0
	STR_PING_INFO ECUInfo_1;
	id = com_shmem_open("/ecu1", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/ecu com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, &ECUInfo_1, sizeof(ECUInfo_1));
	printf("ECU1 connect Status = %d\n", ECUInfo_1.Stat);

	com_shmem_close(id);
#endif

/* ECU */
#if 0
	STR_PING_INFO ECUInfo_2;
	id = com_shmem_open("/ecu2", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/ecu com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, &ECUInfo_2, sizeof(ECUInfo_2));
	printf("ECU2 connect Status = %d\n", ECUInfo_2.Stat);

	com_shmem_close(id);
	printf("\n");
#endif

/* Mavlink */
#if 0
	mavlinkRecv MavlinkRecv;
	id = com_shmem_open("/mavlink_recv", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/mavlink_recv com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, &MavlinkRecv, sizeof(MavlinkRecv));
	printf("Mavlink.Stat = %d\n", MavlinkRecv.Stat);
	printf("Mavlink.Mavlink.sys_time.time_unix_usec = %ld\n", MavlinkRecv.sys_time.time_unix_usec);
	printf("Mavlink.Mavlink.sys_time.time_boot_ms = %d\n", MavlinkRecv.sys_time.time_boot_ms);

	com_shmem_close(id);
	printf("\n");
#endif

/* Camera */
#if 0
	cameraStat *pCameraStat;
	pCameraStat = malloc(sizeof(cameraStat));

	id = com_shmem_open("/readcam0", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/camera com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, pCameraStat, sizeof(cameraStat));
	printf("camera0 stat = %d\n", pCameraStat->Stat);
	printf("camera0 timestamp = %lld\n", pCameraStat->timestamp);
	printf("camera0 img_data = ");
	for(int i = 0; i < 50; i++)
	{
		printf("%02x", pCameraStat->img_data[i]);
	}
	printf("\n");

	free(pCameraStat);

	com_shmem_close(id);
	printf("\n");
#endif

#if 0
	pCameraStat = malloc(sizeof(cameraStat));

	id = com_shmem_open("/readcam1", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/camera com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, pCameraStat, sizeof(cameraStat));
	printf("camera1 stat = %d\n", pCameraStat->Stat);
	printf("camera1 timestamp = %lld\n", pCameraStat->timestamp);
	printf("camera1 img_data = ");
	for(int i = 0; i < 50; i++)
	{
		printf("%02x", pCameraStat->img_data[i]);
	}
	printf("\n");

	free(pCameraStat);

	com_shmem_close(id);
	printf("\n");
#endif

#if 0
	pCameraStat = malloc(sizeof(cameraStat));

	id = com_shmem_open("/readcam2", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/camera com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, pCameraStat, sizeof(cameraStat));
	printf("camera2 stat = %d\n", pCameraStat->Stat);
	printf("camera2 timestamp = %lld\n", pCameraStat->timestamp);
	printf("camera2 img_data = ");
	for(int i = 0; i < 50; i++)
	{
		printf("%02x", pCameraStat->img_data[i]);
	}
	printf("\n");

	free(pCameraStat);

	com_shmem_close(id);
	printf("\n");
#endif

#if 0
	pCameraStat = malloc(sizeof(cameraStat));

	id = com_shmem_open("/readcam3", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/camera com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, pCameraStat, sizeof(cameraStat));
	printf("camera3 stat = %d\n", pCameraStat->Stat);
	printf("camera3 timestamp = %lld\n", pCameraStat->timestamp);
	printf("camera3 img_data = ");
	for(int i = 0; i < 50; i++)
	{
		printf("%02x", pCameraStat->img_data[i]);
	}
	printf("\n");

	free(pCameraStat);

	com_shmem_close(id);
	printf("\n");
#endif

#if 0
	pCameraStat = malloc(sizeof(cameraStat));

	id = com_shmem_open("/readcam4", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/camera com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, pCameraStat, sizeof(cameraStat));
	printf("camera4 stat = %d\n", pCameraStat->Stat);
	printf("camera4 timestamp = %lld\n", pCameraStat->timestamp);
	printf("camera4 img_data = ");
	for(int i = 0; i < 50; i++)
	{
		printf("%02x", pCameraStat->img_data[i]);
	}
	printf("\n");

	free(pCameraStat);

	com_shmem_close(id);
	printf("\n");
#endif

#if 0
	pCameraStat = malloc(sizeof(cameraStat));

	id = com_shmem_open("/readcam5", SHM_KIND_PLATFORM);
	if (id == DEF_COM_SHMEM_FALSE) {
		printf("/camera com_shmem_open() error\n");
		return -1;
	}
	com_shmem_read(id, pCameraStat, sizeof(cameraStat));
	printf("camera5 stat = %d\n", pCameraStat->Stat);
	printf("camera5 timestamp = %lld\n", pCameraStat->timestamp);
	printf("camera5 img_data = ");
	for(int i = 0; i < 50; i++)
	{
		printf("%02x", pCameraStat->img_data[i]);
	}
	printf("\n");

	free(pCameraStat);

	com_shmem_close(id);
	printf("\n");
#endif

//	com_shmem_destroy();
	
	return 0;
}
