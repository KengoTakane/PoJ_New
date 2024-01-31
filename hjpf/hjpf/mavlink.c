#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"
#include "resource.h"
#include "mavlink.h"

/*============================================================================*/
/* global */
/*============================================================================*/
Mavlink_Messages messages;
autopilot_Interface Autopilot_Interface;
mavlinkRecv MavlinkRecv;
pthread_t read_thread_id, write_thread_id;
int reading_status = 0;
int time_to_exit = MAVLINK_FALSE;
extern int gComm_StopFlg;

/*============================================================================*/
/* prototype */
/*============================================================================*/
static uint64_t get_time_usec();
static int write_port(int fd, char* buf, unsigned len);
static int write_message(int fd, const mavlink_message_t *message);
static int write_setpoint(int fd);
static int read_message(int fd, mavlink_message_t *message);
static void read_messages(int fd, int* timeout_cnt);
static void* write_thread(void* arg);
static void* read_thread(void* arg);
static int toggle_offboard_control(int fd, int flag);
static void disable_offboard_control(int fd);
static int enable_offboard_control(int fd);
static void update_setpoint(mavlink_set_position_target_local_ned_t setpoint);
static void commands(int fd);
static int Start(int fd);

/*============================================================================*/
/* const */
/*============================================================================*/

/*============================================================================*/
/*
 * @brief   現時刻の取得
 * @note    現在の時刻を取得する．単位は，[us]．
 * @param   引数  : 
 * @return  戻り値: uint64_t
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static uint64_t get_time_usec()
{
	static struct timeval _time_stamp;
	gettimeofday(&_time_stamp, NULL);
	return _time_stamp.tv_sec*1000000 + _time_stamp.tv_usec;
}

/*============================================================================*/
/*
 * @brief   ポートへの書き込み処理
 * @note    ロック→ポートへ書き込み→アンロック
 * @param   引数  : int         fd      ポートのファイルディスクリプタ
 *                  char*       buf　   ポートに書き込むbyte型データ
 *                  unsinged    len     ポートに書き込むbyte型データサイズ
 * @return  戻り値: int         bytesWritten　ポートに書き込まれたデータサイズ[bytes]，-1:失敗．
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int write_port(int fd, char* buf, unsigned len)
{
    pthread_mutex_lock(&Autopilot_Interface.lock);
    
    const int bytesWritten = (int)(write(fd, buf, len));
    
    tcdrain(fd);
    pthread_mutex_unlock(&Autopilot_Interface.lock);

    return bytesWritten;
}

/*============================================================================*/
/*
 * @brief   message書き込み処理
 * @note    指定したポートに指定したmessageを書き込む
 * @param   引数  : int                     fd              ポートのファイルディスクリプタ
 *                  const mavlink_message_t message        ポートに書き込む内容
 * @return  戻り値: static int              bytesWritten    ポートに書き込まれたデータサイズ[bytes]，-1:失敗．
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int write_message(int fd, const mavlink_message_t *message)
{
    char buf[300];
    unsigned len = mavlink_msg_to_send_buffer((uint8_t*)buf, message);  /* messageをbyte型に変換及びサイズ取得 */

    int bytesWritten = write_port(fd, buf, len);

    return bytesWritten;
}

/*============================================================================*/
/*
 * @brief   setpointの書き込み処理
 * @note    ターゲット(target)のセットポジション(set position)を指定したポートに書き込む．
 * @param   引数  : int fd
 * @return  戻り値: int 0:成功，-1:失敗
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int write_setpoint(int fd)
{
    mavlink_set_position_target_local_ned_t sp;
    
    pthread_mutex_lock(&Autopilot_Interface.current_setpoint.mutex);
    sp = Autopilot_Interface.current_setpoint.data;
    pthread_mutex_unlock(&Autopilot_Interface.current_setpoint.mutex);

    if (sp.time_boot_ms != 0)   /* システム起動してから時間が経っている */
    {
        sp.time_boot_ms = (uint32_t) (get_time_usec()/1000);
    }
    sp.target_system    = Autopilot_Interface.system_id;
    sp.target_component = Autopilot_Interface.autopilot_id;

    mavlink_message_t message;
    mavlink_msg_set_position_target_local_ned_encode(Autopilot_Interface.system_id, Autopilot_Interface.companion_id, &message, &sp);

    int len = write_message(fd, &message);
    Autopilot_Interface.write_count++;

	if ( len <= 0 )
    {
        dprintf(ERROR,"WARNING: could not send POSITION_TARGET_LOCAL_NED \n");
        return -1;
    }

    return 0;
}

/*============================================================================*/
/*
 * @brief   messageの読み込み処理
 * @note    messageの読み込み
 * @param   引数  : int                 fd
 *                  mavlink_message_t   message     読み込んだデータ
 * @return  戻り値: int                 msgReceived     0:失敗，1:成功
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int read_message(int fd, mavlink_message_t *message)
{
    int result;
    uint8_t cp;
    uint8_t msgReceived = MAVLINK_FALSE;
    mavlink_status_t status;
    static int timeout_cnt = 0;

    pthread_mutex_lock(&Autopilot_Interface.lock);
    result = read(fd, &cp, 1);
    pthread_mutex_unlock(&Autopilot_Interface.lock);

    if(result > 0)
    {
        //dprintf(ERROR, "read from fd %d\n", fd);
        msgReceived = mavlink_parse_char(MAVLINK_COMM_1, cp, message, &status);
        timeout_cnt = 0;

        MavlinkRecv.Stat = 0;
        com_shmem_write(Autopilot_Interface.id_recv, &MavlinkRecv, sizeof(MavlinkRecv));
    }
    else
    {
        //dprintf(ERROR, "ERROR: Could not read from fd %d\n", fd);
        usleep(1000);
        timeout_cnt += 1;
        
        if(timeout_cnt > Autopilot_Interface.timeout)   /* タイムアウトになった場合 */
        {
            MavlinkRecv.Stat = 1;
            com_shmem_write(Autopilot_Interface.id_recv, &MavlinkRecv, sizeof(MavlinkRecv));
        }
    }

    return msgReceived;
}

/*============================================================================*/
/*
 * @brief   messagesの読み込み処理
 * @note    Ctrl+Cが実行されるまで，messages読込処理を行う．
 * @param   引数  : int fd
 *          引数  : int* timeout_cnt
 * @return  戻り値: void 
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static void read_messages(int fd, int* timeout_cnt)
{
    int success = MAVLINK_FALSE;
    int received_all = MAVLINK_FALSE;
    Time_Stamps this_timestamps;

    while(received_all == MAVLINK_FALSE && gComm_StopFlg == DEF_COMM_OFF)
    {
        mavlink_message_t message;
        success = read_message(fd, &message);

        if(success == MAVLINK_TRUE) /* 読込成功の場合 */
        {
            // Store message sysid and compid.
            // Note this doesn't handle multiple message sources.
            Autopilot_Interface.current_messages.sysid  = message.sysid;
            Autopilot_Interface.current_messages.compid = message.compid;

            // Handle Message ID
            switch (message.msgid)  /* Message ID によってデコード方法が異なる */
            {
                case MAVLINK_MSG_ID_SYSTEM_TIME:
                {
                    mavlink_msg_system_time_decode(&message, &(Autopilot_Interface.current_messages.sys_time));
                    Autopilot_Interface.current_messages.time_stamps.sys_time = get_time_usec();
					this_timestamps.sys_time = Autopilot_Interface.current_messages.time_stamps.sys_time;

                    memcpy(&(MavlinkRecv.sys_time), &(Autopilot_Interface.current_messages.sys_time), sizeof(MavlinkRecv.sys_time));
                    com_shmem_write(Autopilot_Interface.id_recv, &MavlinkRecv, sizeof(MavlinkRecv));
                    break;
                }
                case MAVLINK_MSG_ID_HEARTBEAT:
                {
                    //printf("MAVLINK_MSG_ID_HEARTBEAT\n");
                    mavlink_msg_heartbeat_decode(&message, &(Autopilot_Interface.current_messages.heartbeat));
                    Autopilot_Interface.current_messages.time_stamps.heartbeat = get_time_usec();
					this_timestamps.heartbeat = Autopilot_Interface.current_messages.time_stamps.heartbeat;

                    break;
                }

                case MAVLINK_MSG_ID_SYS_STATUS:
                {
                    //printf("MAVLINK_MSG_ID_SYS_STATUS\n");
                    mavlink_msg_sys_status_decode(&message, &(Autopilot_Interface.current_messages.sys_status));
                    Autopilot_Interface.current_messages.time_stamps.sys_status = get_time_usec();
                    this_timestamps.sys_status = Autopilot_Interface.current_messages.time_stamps.sys_status;
                    break;
                }

                case MAVLINK_MSG_ID_BATTERY_STATUS:
                {
                    //printf("MAVLINK_MSG_ID_BATTERY_STATUS\n");
                    mavlink_msg_battery_status_decode(&message, &(Autopilot_Interface.current_messages.battery_status));
                    Autopilot_Interface.current_messages.time_stamps.battery_status = get_time_usec();
                    this_timestamps.battery_status = Autopilot_Interface.current_messages.time_stamps.battery_status;
                    break;
                }

                case MAVLINK_MSG_ID_RADIO_STATUS:
                {
                    //printf("MAVLINK_MSG_ID_RADIO_STATUS\n");
                    mavlink_msg_radio_status_decode(&message, &(Autopilot_Interface.current_messages.radio_status));
                    Autopilot_Interface.current_messages.time_stamps.radio_status = get_time_usec();
                    this_timestamps.radio_status = Autopilot_Interface.current_messages.time_stamps.radio_status;
                    break;
                }

                case MAVLINK_MSG_ID_LOCAL_POSITION_NED:
                {
                    //printf("MAVLINK_MSG_ID_LOCAL_POSITION_NED\n");
                    mavlink_msg_local_position_ned_decode(&message, &(Autopilot_Interface.current_messages.local_position_ned));
                    Autopilot_Interface.current_messages.time_stamps.local_position_ned = get_time_usec();
                    this_timestamps.local_position_ned = Autopilot_Interface.current_messages.time_stamps.local_position_ned;
                    break;
                }

                case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
                {
                    //printf("MAVLINK_MSG_ID_GLOBAL_POSITION_INT\n");
                    mavlink_msg_global_position_int_decode(&message, &(Autopilot_Interface.current_messages.global_position_int));
                    Autopilot_Interface.current_messages.time_stamps.global_position_int = get_time_usec();
                    this_timestamps.global_position_int = Autopilot_Interface.current_messages.time_stamps.global_position_int;
                    break;
                }

                case MAVLINK_MSG_ID_POSITION_TARGET_LOCAL_NED:
                {
                    //printf("MAVLINK_MSG_ID_POSITION_TARGET_LOCAL_NED\n");
                    mavlink_msg_position_target_local_ned_decode(&message, &(Autopilot_Interface.current_messages.position_target_local_ned));
                    Autopilot_Interface.current_messages.time_stamps.position_target_local_ned = get_time_usec();
                    this_timestamps.position_target_local_ned = Autopilot_Interface.current_messages.time_stamps.position_target_local_ned;
                    break;
                }

                case MAVLINK_MSG_ID_POSITION_TARGET_GLOBAL_INT:
                {
                    //printf("MAVLINK_MSG_ID_POSITION_TARGET_GLOBAL_INT\n");
                    mavlink_msg_position_target_global_int_decode(&message, &(Autopilot_Interface.current_messages.position_target_global_int));
                    Autopilot_Interface.current_messages.time_stamps.position_target_global_int = get_time_usec();
                    this_timestamps.position_target_global_int = Autopilot_Interface.current_messages.time_stamps.position_target_global_int;
                    break;
                }

                case MAVLINK_MSG_ID_HIGHRES_IMU:
                {
                    //printf("MAVLINK_MSG_ID_HIGHRES_IMU\n");
                    mavlink_msg_highres_imu_decode(&message, &(Autopilot_Interface.current_messages.highres_imu));
                    Autopilot_Interface.current_messages.time_stamps.highres_imu = get_time_usec();
                    this_timestamps.highres_imu = Autopilot_Interface.current_messages.time_stamps.highres_imu;
                    break;
                }

                case MAVLINK_MSG_ID_ATTITUDE:
                {
                    //printf("MAVLINK_MSG_ID_ATTITUDE\n");
                    mavlink_msg_attitude_decode(&message, &(Autopilot_Interface.current_messages.attitude));
                    Autopilot_Interface.current_messages.time_stamps.attitude = get_time_usec();
                    this_timestamps.attitude = Autopilot_Interface.current_messages.time_stamps.attitude;
                    break;
                }
                
                default:
                {
                    // printf("Warning, did not handle message id %i\n",message.msgid);
                    break;
                }
            }
        }

        received_all = this_timestamps.heartbeat && this_timestamps.sys_status;

        if (Autopilot_Interface.writing_status > false ) {
            usleep(100);
        }
    }
}

/*============================================================================*/
/*
 * @brief   書き込み処理
 * @note    書き込み処理用のスレッド
 * @param   引数  : void*   arg
 * @return  戻り値: void*
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static void* write_thread(void* arg)
{    
    int fd = *(int *)arg;
    int ret;
    int timeout_cnt = 0;

    //enable_offboard_control(fd);

    if (Autopilot_Interface.writing_status != MAVLINK_FALSE )
	{
		dprintf(INFO,"write thread already running\n");
		return NULL;
	}

    Autopilot_Interface.writing_status = 2;
    
    mavlink_set_position_target_local_ned_t sp;
	sp.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_VELOCITY &
				   MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_YAW_RATE;
	sp.coordinate_frame = MAV_FRAME_LOCAL_NED;
	sp.vx       = 0.0;
	sp.vy       = 0.0;
	sp.vz       = 0.0;
	sp.yaw_rate = 0.0;
    
    pthread_mutex_lock(&Autopilot_Interface.current_setpoint.mutex);
    Autopilot_Interface.current_setpoint.data = sp;
    pthread_mutex_unlock(&Autopilot_Interface.current_setpoint.mutex);

    write_setpoint(fd);
	Autopilot_Interface.writing_status = true;

    com_timer_init(ENUM_TIMER_MAVW, Autopilot_Interface.write_period);

	while (gComm_StopFlg == DEF_COMM_OFF)
	{
		ret = write_setpoint(fd);

        com_mtimer(ENUM_TIMER_MAVW);
        timeout_cnt += Autopilot_Interface.write_period;

        if(ret == 0)
        {
            MavlinkRecv.Stat = 0;
            timeout_cnt = 0;
            com_shmem_write(Autopilot_Interface.id_recv, &MavlinkRecv, sizeof(MavlinkRecv));
            continue;
        }

        if(timeout_cnt > Autopilot_Interface.timeout)
        {
            MavlinkRecv.Stat = 1;
            com_shmem_write(Autopilot_Interface.id_recv, &MavlinkRecv, sizeof(MavlinkRecv));
        }
	}

	// signal end
	Autopilot_Interface.writing_status = false;

    return NULL;
}

/*============================================================================*/
/*
 * @brief   読み込み処理
 * @note    読み込み処理用のスレッド
 * @param   引数  : void* arg
 * @return  戻り値: void*
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static void* read_thread(void* arg)
{    
    int fd = *(int *)arg;
    int timeout_cnt = 0;

    if(reading_status != 0)
    {
        dprintf(ERROR, "read thread already running\n");
        return NULL;
    }
    
    reading_status = MAVLINK_TRUE;

    while(gComm_StopFlg == DEF_COMM_OFF)
    {
        read_messages(fd, &timeout_cnt);
        usleep(10000);
        timeout_cnt += 10000;
    }

    reading_status = MAVLINK_FALSE;
 
    return NULL;
}

/*============================================================================*/
/*
 * @brief   オフボード制御切り替え
 * @note    
 * @param   引数  : int fd
 *                  int flag
 * @return  戻り値: int len ポートに書き込まれたデータサイズ[bytes]，-1:失敗
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int toggle_offboard_control(int fd, int flag)
{
    mavlink_command_long_t com = { 0 };
	com.target_system    = Autopilot_Interface.system_id;
	com.target_component = Autopilot_Interface.autopilot_id;
	com.command          = MAV_CMD_NAV_GUIDED_ENABLE;
	com.confirmation     = 1;
	com.param1           = (float) flag;

    mavlink_message_t message;
    mavlink_msg_command_long_encode(Autopilot_Interface.system_id, Autopilot_Interface.companion_id, &message, &com);

    int len = write_message(fd, &message);

    return len;
}

/*============================================================================*/
/*
 * @brief   オフボード制御無効化
 * @note    
 * @param   引数  : int fd
 * @return  戻り値: void
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static void disable_offboard_control(int fd)
{
    if(Autopilot_Interface.control_status == MAVLINK_TRUE)
    {
        int success = toggle_offboard_control(fd, 0);   /* flag=0より無効化 */

        if(success)
        {
            Autopilot_Interface.control_status = MAVLINK_FALSE;
        }
        else
        {
            dprintf(INFO,"Error: off-board mode not set, could not write message\n");
        }
    }
}

/*============================================================================*/
/*
 * @brief   オフボード制御有効化
 * @note    
 * @param   引数  : int fd
 * @return  戻り値: void
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int enable_offboard_control(int fd)
{
    if(Autopilot_Interface.control_status == MAVLINK_FALSE)
    {
        int success = toggle_offboard_control(fd, 1);

        if(success != -1)
        {
            Autopilot_Interface.control_status = MAVLINK_TRUE;
        }
        else
        {
            dprintf(ERROR,"Error: off-board mode not set, could not write message\n");
        }
    }
    return 0;
}

/*============================================================================*/
/*
 * @brief   position更新
 * @note    
 * @param   引数  : mavlink_set_position_target_local_ned_t setpoint
 * @return  戻り値: void
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static void update_setpoint(mavlink_set_position_target_local_ned_t setpoint)
{
    pthread_mutex_lock(&Autopilot_Interface.current_setpoint.mutex);
    Autopilot_Interface.current_setpoint.data = setpoint;
    pthread_mutex_unlock(&Autopilot_Interface.current_setpoint.mutex);
}

/*============================================================================*/
/*
 * @brief   コマンド処理
 * @note    共有メモリより値の読み込み、各値のセット
 * @param   引数  : int fd
 * @return  戻り値: int 
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static void commands(int fd)
{
    enable_offboard_control(fd);
    usleep(100);
    static double local_timestamp;

    mavlink_set_position_target_local_ned_t sp;
    mavlinkSend MavlinkSend;

    com_shmem_read(Autopilot_Interface.id_send, &MavlinkSend, sizeof(MavlinkSend));

    if(local_timestamp == MavlinkSend.timestamp)
    {
        return;
    }

    local_timestamp = MavlinkSend.timestamp;

    printf("%f, %f, %f, %f\n",  MavlinkSend.vx,  MavlinkSend.vy,  MavlinkSend.vz,  MavlinkSend.yaw_rate);

    sp.vx = MavlinkSend.vx;
    sp.vy = MavlinkSend.vy;
    sp.vz = MavlinkSend.vz;
    sp.yaw_rate = MavlinkSend.yaw_rate;

    sp.type_mask = MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_VELOCITY & MAVLINK_MSG_SET_POSITION_TARGET_LOCAL_NED_YAW_RATE;
    sp.coordinate_frame = MAV_FRAME_LOCAL_NED;

    update_setpoint(sp);

    for(int i = 0; i < 8; i++)
    {
        mavlink_local_position_ned_t pos = Autopilot_Interface.current_messages.local_position_ned;
        dprintf(INFO, "%i CURRENT POSITION XYZ = [ % .4f , % .4f , % .4f ] \n", i, pos.x, pos.y, pos.z);
		sleep(1);
    }

    disable_offboard_control(fd);

    // copy current messages
	Mavlink_Messages messages = Autopilot_Interface.current_messages;

	// local position in ned frame
	mavlink_local_position_ned_t pos = messages.local_position_ned;
	dprintf(INFO, "Got message LOCAL_POSITION_NED (spec: https://mavlink.io/en/messages/common.html#LOCAL_POSITION_NED)\n");
	dprintf(INFO, "    pos  (NED):  %f %f %f (m)\n", pos.x, pos.y, pos.z );

	// hires imu
	mavlink_highres_imu_t imu = messages.highres_imu;
	dprintf(INFO, "Got message HIGHRES_IMU (spec: https://mavlink.io/en/messages/common.html#HIGHRES_IMU)\n");
	dprintf(INFO, "    ap time:     %lu \n", imu.time_usec);
	dprintf(INFO, "    acc  (NED):  % f % f % f (m/s^2)\n", imu.xacc , imu.yacc , imu.zacc );
	dprintf(INFO, "    gyro (NED):  % f % f % f (rad/s)\n", imu.xgyro, imu.ygyro, imu.zgyro);
	dprintf(INFO, "    mag  (NED):  % f % f % f (Ga)\n"   , imu.xmag , imu.ymag , imu.zmag );
	dprintf(INFO, "    baro:        %f (mBar) \n"  , imu.abs_pressure);
	dprintf(INFO, "    altitude:    %f (m) \n"     , imu.pressure_alt);
	dprintf(INFO, "    temperature: %f C \n"       , imu.temperature );
}

/*============================================================================*/
/*
 * @brief   初期処理
 * @note    スレッドの生成
 * @param   引数  : int fd
 * @return  戻り値: int ret
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int Start(int fd)
{
    int ret;

    dprintf(INFO, "START READ THREAD \n");

    // readスレッド生成
    ret = pthread_create(&read_thread_id, NULL, &read_thread, &fd);
    if (ret != DEF_RET_OK) {
        dprintf(WARN, "read thread create error=%d\n", errno);
        return DEF_RET_NG;
    }

    dprintf(INFO, "CHECK FOR MESSAGES\n");

    while(Autopilot_Interface.current_messages.sysid != 0)
    {
        usleep(10000);  /* sleep for 10s */
    }

    if(Autopilot_Interface.system_id)
    {
        Autopilot_Interface.system_id = Autopilot_Interface.current_messages.sysid;
        dprintf(INFO, "GOT VEHICLE SYSTEM ID: %d\n", Autopilot_Interface.current_messages.sysid );
    }

    if(Autopilot_Interface.autopilot_id)
    {
        Autopilot_Interface.autopilot_id = Autopilot_Interface.current_messages.compid;
        dprintf(INFO, "GOT AUTOPILOT COMPONENT ID: %i\n\n", Autopilot_Interface.autopilot_id);
    }

    Mavlink_Messages local_data = Autopilot_Interface.current_messages;
    Autopilot_Interface.initial_position.x        = local_data.local_position_ned.x;
	Autopilot_Interface.initial_position.y        = local_data.local_position_ned.y;
	Autopilot_Interface.initial_position.z        = local_data.local_position_ned.z;
	Autopilot_Interface.initial_position.vx       = local_data.local_position_ned.vx;
	Autopilot_Interface.initial_position.vy       = local_data.local_position_ned.vy;
	Autopilot_Interface.initial_position.vz       = local_data.local_position_ned.vz;
	Autopilot_Interface.initial_position.yaw      = local_data.attitude.yaw;
	Autopilot_Interface.initial_position.yaw_rate = local_data.attitude.yawspeed;

    dprintf(INFO, "INITIAL POSITION XYZ = [ %.4f , %.4f , %.4f ] \n", Autopilot_Interface.initial_position.x, Autopilot_Interface.initial_position.y, Autopilot_Interface.initial_position.z);
	dprintf(INFO, "INITIAL POSITION YAW = %.4f \n\n", Autopilot_Interface.initial_position.yaw);

    dprintf(INFO, "START WRITE THREAD \n");

    // writeスレッド生成
    ret = pthread_create(&write_thread_id, NULL, &write_thread, &fd);
    if (ret != DEF_RET_OK) {
        dprintf(WARN, "write thread create error=%d\n", errno);
        return DEF_RET_NG;
    }

    return 0;
}

/*============================================================================*/
/*
 * @brief   ボーレート設定
 * @note    ボーレートの値を設定する．
 * @param   引数  : resMavlinkInfo *resMavlink
 * @return  戻り値: int ret     0以上:ボーレートの値, -1:設定失敗
 * @date    2023/12/15 [0.0.1] 新規作成
 */
/*============================================================================*/
static int SetupBaudrate(resMavlinkInfo *resMavlink)
{
    int ret;

    switch (resMavlink->baudrate)
	{
		case 1200:
            ret = B1200;
			break;
		case 1800:
			ret = B1800;
			break;
		case 9600:
			ret = B9600;
			break;
		case 19200:
			ret = B19200;
			break;
		case 38400:
			ret = B38400;
			break;
		case 57600:
			ret = B57600;
			break;
		case 115200:
			ret = B115200;
			break;
		case 460800:
			ret = B460800;
			break;
		case 921600:
			ret = B921600;
			break;
		default:
			ret = -1;
			break;
	}

    return ret;
}

/*============================================================================*/
/*
 * @brief   メイン処理
 * @note    メイン処理
 * @param   引数  : void* arg
 * @return  戻り値: void*
 * @date    2023/12/15 [0.0.1] 新規作成
 * @date    2024/01/25 [0.0.2] デバッグ出力修正
 */
/*============================================================================*/
void* MavlinkMain(void* arg)
{
    int fd;
    int ret;
    int baudrate;

    resMavlinkInfo *resMavlink = (resMavlinkInfo*)arg;
    Autopilot_Interface.read_period = resMavlink->read_period;
    Autopilot_Interface.write_period = resMavlink->write_period;
    Autopilot_Interface.timeout = resMavlink->timeout;

    com_timer_init(ENUM_TIMER_MAVM, 1000);  /* スリープ周期 : 1[s] */

    baudrate = SetupBaudrate(resMavlink);
    if(baudrate == -1)
    {
        dprintf(ERROR, "baudrate = %d. error set mavlink baudrate.\n", resMavlink->baudrate);
        pthread_exit(NULL); 
    }

    fd = com_serial_open(resMavlink->devname, baudrate);
    if(fd == DEF_RET_NG)
    {
        MavlinkRecv.Stat = 1;   /* 受信異常 */
        dprintf(ERROR, "fail to com_serial_open(%s, %d)\n", resMavlink->devname, resMavlink->baudrate);
        com_shmem_write(Autopilot_Interface.id_recv, &MavlinkRecv, sizeof(MavlinkRecv));

        pthread_exit(NULL);    
    }

    /* 共有メモリオープン(受信) */
    Autopilot_Interface.id_recv = com_shmem_open(DEF_MAVLINK_RECV_SHMEM_NAME, SHM_KIND_PLATFORM);
    if(Autopilot_Interface.id_recv == DEF_RET_NG)
    {
        com_serial_close(fd);
        dprintf(ERROR, "com_shmem_open error(mavlink recv). name = %s\n", DEF_MAVLINK_RECV_SHMEM_NAME);
		pthread_exit(NULL);   
    }

    /* 共有メモリオープン(送信) */
    Autopilot_Interface.id_send = com_shmem_open(DEF_MAVLINK_SEND_SHMEM_NAME, SHM_KIND_USER);
    if(Autopilot_Interface.id_send == DEF_RET_NG)
    {
        com_serial_close(fd);
        dprintf(ERROR, "com_shmem_open error(mavlink send). name = %s\n", DEF_MAVLINK_SEND_SHMEM_NAME);
		pthread_exit(NULL);   
    }

    int result_lock = pthread_mutex_init(&Autopilot_Interface.lock, NULL);
	if ( result_lock != 0 )
	{
		printf("\n mutex init failed\n");
	}

    int result_mutex = pthread_mutex_init(&Autopilot_Interface.current_setpoint.mutex, NULL);
	if ( result_mutex != 0 )
	{
		printf("\n mutex init failed\n");
	}

    Start(fd);

    while(gComm_StopFlg == DEF_COMM_OFF)
    {
        commands(fd);
        com_mtimer(ENUM_TIMER_MAVM);
    }

    //スレッド終了待ち
    void *pret;
    ret = pthread_join(read_thread_id, &pret);
    if (ret != DEF_RET_OK) {
        dprintf(WARN, "pthread_join error=%d\n", errno);
    }

     ret = pthread_join(write_thread_id, &pret);
    if (ret != DEF_RET_OK) {
        dprintf(WARN, "pthread_join error=%d\n", errno);
    }

    //共有メモリクローズ
    com_shmem_close(Autopilot_Interface.id_recv);
    com_shmem_close(Autopilot_Interface.id_send);

    //シリアルポートクローズ
    com_serial_close(fd);

    pthread_mutex_destroy(&Autopilot_Interface.lock);
    pthread_mutex_destroy(&Autopilot_Interface.current_setpoint.mutex);

    pthread_exit(NULL);

}
