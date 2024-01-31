/*============================================================================*/
/*
 * @file    mavlink.h
 * @brief   CubePilot
 * @note    CubePilot
 * @date    2023/12/14
 */
/*============================================================================*/
#ifndef __INS_MAVLINK_H
#define __INS_MAVLINK_H
/*============================================================================*/
/* include */
/*============================================================================*/
#include <stdio.h>
#include <common/mavlink.h>

/*============================================================================*/
/* define */
/*============================================================================*/
#define MAVLINK_TRUE (1)
#define MAVLINK_FALSE (0)
#define DEF_MAVLINK_SEND_SHMEM_NAME "/mavlink_send"
#define DEF_MAVLINK_RECV_SHMEM_NAME "/mavlink_recv"
#define DEF_TIMER_KIND_MAVLINK (5)
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_POSITION     0b0000110111111000
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_VELOCITY     0b0000110111000111
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_ACCELERATION 0b0000110000111111
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_FORCE        0b0000111000111111
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_YAW_ANGLE    0b0000100111111111
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_YAW_RATE     0b0000010111111111

#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_TAKEOFF      0x1000
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_LAND         0x2000
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_LOITER       0x3000
#define MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_IDLE         0x4000

/*============================================================================*/
/* typedef */
/*============================================================================*/
typedef struct _Time_Stamps{
    uint64_t heartbeat;
    uint64_t sys_status;
    uint64_t battery_status;
    uint64_t radio_status;
    uint64_t local_position_ned;
    uint64_t global_position_int;
    uint64_t position_target_local_ned;
    uint64_t position_target_global_int;
    uint64_t highres_imu;
    uint64_t attitude;
	uint64_t sys_time;
} Time_Stamps;

typedef struct _Current_setpoint{
    pthread_mutex_t  mutex;
    mavlink_set_position_target_local_ned_t data;
} Current_setpoint;


typedef struct _Mavlink_Messages{
    int sysid;
	int compid;

	mavlink_heartbeat_t heartbeat;
	mavlink_sys_status_t sys_status;
	mavlink_battery_status_t battery_status;
	mavlink_radio_status_t radio_status;
	mavlink_local_position_ned_t local_position_ned;
	mavlink_global_position_int_t global_position_int;
	mavlink_position_target_local_ned_t position_target_local_ned;
	mavlink_position_target_global_int_t position_target_global_int;
	mavlink_highres_imu_t highres_imu;
	mavlink_attitude_t attitude;
	mavlink_system_time_t sys_time;
    Time_Stamps time_stamps;

} Mavlink_Messages;

typedef struct _autopilot_Interface{
    char reading_status;
	char writing_status;
	char control_status;
    uint64_t write_count;

    int system_id;
	int autopilot_id;
	int companion_id;

	Mavlink_Messages current_messages;
	mavlink_set_position_target_local_ned_t initial_position;

    int time_to_exit;
    Current_setpoint current_setpoint;
	pthread_t read_thread_id, write_thread_id;
	pthread_mutex_t  lock;	/* exclusive lock : LOCK or UNLOCK */
	int id_recv, id_send;	/* 共有メモリID */

	int read_period;		/* 読み込み周期 */
	int write_period;		/* 書き込み周期 */
	int timeout;			/* タイムアウトにする時間[?] */
} autopilot_Interface;

typedef struct _mavlinkSend{	/* 共有メモリに書き込む送信用情報 */
	double timestamp;			/* タイムスタンプ */
	float vx;					/* X速度 */
	float vy;					/* Y速度 */
	float vz;					/* Z速度 */
	float yaw_rate;				/* 方位角レート */
} mavlinkSend;

typedef struct _mavlinkRecv{	/* 共有メモリに書き込む受信用情報 */
	int32_t Stat;				/* 0:通信正常、1:通信異常 */
	mavlink_system_time_t sys_time;
} mavlinkRecv;

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
extern void* MavlinkMain(void* arg);

/*============================================================================*/
/* Macro */
/*============================================================================*/

#endif /* __INS_MAVLINK_H */
