/*============================================================================*/
/*
 * @file    camera.c
 * @brief   カメラ
 * @note    カメラ
 * @date    2023/12/08
 */
/*============================================================================*/

/*============================================================================*/
/* include */
/*============================================================================*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/time.h>
#include <sched.h>
#include "process.h"
#include "com_timer.h"
#include "com_shmem.h"
#include "debug.h"
#include "hjpf.h"
#include "camera.h"
#include "resource.h"

/*============================================================================*/
/* global */
/*============================================================================*/
extern int gComm_StopFlg;
static cameraInfo g_CameraInfo[6];
static cameraStat *g_CameraStat[6];

/*============================================================================*/
/* prototype */
/*============================================================================*/

/*============================================================================*/
/* const */
/*============================================================================*/
static int xioctl(int _fd, int request, void *arg);
static void close_device(int _fd);
static int stream_stop(int _fd);
static int unmap_buffer(size_t buffer_size, struct buffer *_buffer);
static int dequeue_buffer(int _fd);
static int start_streaming(int _fd);
static int enqueue_buffer(int _fd, size_t index);
static int enqueue_buffers(int _fd, size_t buffer_size);
static int map_buffer(int _fd, size_t buffer_size, struct buffer *_buffer);
static int set_camera_buffer(int _fd, size_t buffer_size);
static int set_camera_format(int _fd, __u32 img_height, __u32 img_width);
static int is_camera(int _fd);
static int CameraRun(cameraInfo *CameraInfo, size_t cameraNum);
static int OpenDevice(cameraInfo *CameraInfo);
static int CameraStart(size_t cameraNum);
static void CameraEnd(size_t cameraNum);
static int CameraInit(size_t camera_num);

/*============================================================================*/
/*
 * @brief   システムコール
 * @note    スペシャルファイルを操作する
 * @param   引数  : _fd     ファイルディスクリプタ（スペシャルファイル）
 *                  request 操作内容
 *                  arg     操作パラメータ
 *
 * @return  戻り値: -1以外  正常終了
 *                  -1      異常終了
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int xioctl(int _fd, int request, void *arg)
{
    dprintf(INFO, "xioctl(%d, %d, %s)\n", _fd, request, arg);
    int r;
    do
        r = ioctl(_fd, request, arg);
    while (-1 == r && EINTR == errno);
    return r;
}

/*============================================================================*/
/*
 * @brief   デバイスクローズ
 * @note    デバイスをクローズする
 * @param   引数  : _fd     ファイルディスクリプタ（カメラデバイス）
 *
 * @return  戻り値: なし
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static void close_device(int _fd)
{
    dprintf(INFO, "close_device(%d)\n", _fd);
    close(_fd);
}

/*============================================================================*/
/*
 * @brief   ストリーミング停止
 * @note    ストリーミングを停止する
 * @param   引数  : _fd     ファイルディスクリプタ（カメラデバイス）
 *
 * @return  戻り値: true    ストリーミング停止成功
 *                  false   ストリーミング停止失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int stream_stop(int _fd)
{
  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == _fd)
  {
    // perror("No device to stop streaming");
    // std::cerr << "No device to stop streaming" << std::endl;
    return DEF_RET_NG;
  }
  if (-1 == xioctl(_fd, VIDIOC_STREAMOFF, &type))
  {
    perror("failed to stop streaming");
    return DEF_RET_NG;
  }
  return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   バッファ解放
 * @note    呼出元のバッファメモリをアンマッピングする
 * @param   引数  : buffer_size   バッファする画像の枚数
 *                  _buffer       呼出元のバッファメモリ
 *
 * @return  戻り値: true    メモリ解放成功
 *                  false   メモリ解放失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int unmap_buffer(size_t buffer_size, struct buffer *_buffer)
{
  for (size_t i = 0; i < buffer_size; ++i)
  {
    if (0 == _buffer[i].length)
    {
      // do nothing
      ;
    }
    else if (-1 == munmap(_buffer[i].start, _buffer[i].length))
    {
      perror("failed to munmap buffer memory");
      return DEF_RET_NG;
    }
  }
  return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   バッファデキュー
 * @note    バッファをデキューする
 * @param   引数  : _fd     ファイルディスクリプタ（カメラデバイス）
 *
 * @return  戻り値: -1以外  デキューされたバッファの番号
 *                  -1      デキュー失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int dequeue_buffer(int _fd)
{
  struct pollfd fds[1];
  fds[0].fd = _fd;
  fds[0].events = POLLIN;
  int p = poll(fds, 1, 5000);
  if (-1 == p)
  {
    perror("Waiting for Frame");
    return DEF_RET_NG;
  }

  struct v4l2_buffer buf = {0};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (-1 == xioctl(_fd, VIDIOC_DQBUF, &buf))
  {
    perror("Retrieving Frame");
    return DEF_RET_NG;
  }
  return buf.index;
}

/*============================================================================*/
/*
 * @brief   ストリーミング開始
 * @note    ストリーミングを開始する（バッファのデキュー許可を要求する）
 * @param   引数  : _fd     ファイルディスクリプタ（カメラデバイス）
 *
 * @return  戻り値: true    ストリーミング開始成功
 *                  false   ストリーミング開始失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int start_streaming(int _fd)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(_fd, VIDIOC_STREAMON, &type))
    {
        perror("failed to start streaming");
        return DEF_RET_NG;
    }
    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   バッファエンキュー
 * @note    指定バッファに画像をエンキューする
 * @param   引数  : _fd     ファイルディスクリプタ（カメラデバイス）
 *                  index   エンキューするバッファの番号
 *
 * @return  戻り値: true    エンキュー成功
 *                  false   エンキュー失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int enqueue_buffer(int _fd, size_t index)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    if (-1 == xioctl(_fd, VIDIOC_QBUF, &buf))
    {
        perror("failed to enqueue buffer");
        return DEF_RET_NG;
    }
    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   全バッファエンキュー
 * @note    バッファいっぱいに画像をエンキューする
 * @param   引数  : _fd           ファイルディスクリプタ（カメラデバイス）
 *                  buffer_size   エンキューする画像の枚数
 *
 * @return  戻り値: true    エンキュー成功
 *                  false   エンキュー失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int enqueue_buffers(int _fd, size_t buffer_size)
{
    for (size_t index = 0; index < buffer_size; index++)
    {
        if (enqueue_buffer(_fd, index) == DEF_RET_NG)
        {
        return DEF_RET_NG;
        }
    }
    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   バッファマッピング
 * @note    カメラ側のバッファメモリを呼出元メモリに対応付ける
 * @param   引数  : _fd           ファイルディスクリプタ（カメラデバイス）
 *                  buffer_size   バッファする画像の枚数
 *                  _buffer       呼出元のバッファメモリ
 *
 * @return  戻り値: true    対応付け成功
 *                  false   対応付け失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int map_buffer(int _fd, size_t buffer_size, struct buffer *_buffer)
{
    for (size_t index = 0; index < buffer_size; index++)
    {
        struct v4l2_buffer buff = {0};
        buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buff.memory = V4L2_MEMORY_MMAP;
        buff.index = index;
        if (-1 == xioctl(_fd, VIDIOC_QUERYBUF, &buff))
        {
        perror("failed to query buffer");
        return DEF_RET_NG;
        }
        _buffer[index].length = buff.length;
        _buffer[index].start = mmap(NULL,
                                    buff.length,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    _fd,
                                    buff.m.offset);
    }
    return DEF_RET_OK;
    }

/*============================================================================*/
/*
 * @brief   カメラバッファ設定
 * @note    デバイスファイルに，カメラ側のメモリに持たせるバッファを設定する
 * @param   引数  : _fd           ファイルディスクリプタ（カメラデバイス）
 *                  buffer_size   バッファする画像の枚数
 *
 * @return  戻り値: true    設定成功
 *                  false   設定失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int set_camera_buffer(int _fd, size_t buffer_size)
{
    struct v4l2_requestbuffers req = {0};
    req.count = buffer_size;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (-1 == xioctl(_fd, VIDIOC_REQBUFS, &req))
    {
        perror("failed to set buffer memory on camera");
        return DEF_RET_NG;
    }
    /* check secured buffer num */
    if (req.count < buffer_size)
    {
        perror("insufficient buffer memory on camera");
        return DEF_RET_NG;
    }
    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   カメラフォーマット設定
 * @note    デバイスファイルに，カメラ画像のピクセル形式、横幅、高さを指定する．
 * @param   引数  : _fd         ファイルディスクリプタ（カメラデバイス）
 *                  img_height  画像の高さ
 *                  img_width   画像の幅
 *
 * @return  戻り値: 0    設定成功
 *                  -1   設定失敗
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int set_camera_format(int _fd, __u32 img_height, __u32 img_width)
{
    struct v4l2_format fmt = {0};
    __u32 pixelformat = (__u32)V4L2_PIX_FMT_UYVY;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.height = img_height;
    fmt.fmt.pix.width = img_width;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    /* set format */
    if (-1 == xioctl(_fd, VIDIOC_S_FMT, &fmt))
    {
        perror("failed to set pixel format");
        return DEF_RET_NG;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    /* get format */
    if (-1 == xioctl(_fd, VIDIOC_G_FMT, &fmt))
    {
        perror("failed to get pixcel format");
        return DEF_RET_NG;
    }
    if (fmt.fmt.pix.pixelformat != pixelformat)
    {
        printf("current pixcel format (%d) can not set to %d \n", fmt.fmt.pix.pixelformat, pixelformat);
        return DEF_RET_NG;
    }
    if (fmt.fmt.pix.height != img_height)
    {
        printf("current height (%d) can not set to %d \n", fmt.fmt.pix.height, img_height);
        return DEF_RET_NG;
    }
    if (fmt.fmt.pix.width != img_width)
    {
        printf("current width (%d) can not set to %d \n", fmt.fmt.pix.width, img_width);
        return DEF_RET_NG;
    }
    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   カメラ機能確認
 * @note    デバイスにカメラ機能があることを確認する
 * @param   引数  : _fd     ファイルディスクリプタ（カメラデバイス）
 *
 * @return  戻り値: 0    機能あり
 *                  -1   機能なし
 * @date    2023/12/08 [1.0.0] 
 */
/*============================================================================*/
static int is_camera(int _fd)
{
    struct v4l2_capability cap;
    if (-1 == xioctl(_fd, VIDIOC_QUERYCAP, &cap))
    {
        perror("failed to query capability");
        return DEF_RET_NG;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        perror("device does not support video capture");
        return DEF_RET_NG;
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        perror("device does not support stream");
        return DEF_RET_NG;
    }
    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   カメラ読込実行
 * @note    カメラ読込を実行する
 * @param   引数  :
 *                  
 * @return  戻り値: int
 * @date    2023/12/08 [0.0.1] 新規作成
 */
/*============================================================================*/
static int CameraRun(cameraInfo *CameraInfo, size_t cameraNum)
{
    int timeout_cnt = 0;

    if (is_camera(CameraInfo->fd) == DEF_RET_NG)
    {
        dprintf(ERROR, "%d does not have camera capability.", cameraNum);
        return DEF_RET_NG;
    }

    if (set_camera_format(CameraInfo->fd, CameraInfo->img_height, CameraInfo->img_width) == DEF_RET_NG)
    {
        dprintf(ERROR, "%d can not set camera format.", cameraNum);
        return DEF_RET_NG;
    }

    if (set_camera_buffer(CameraInfo->fd, CameraInfo->buffer_size) == DEF_RET_NG)
    {
        dprintf(ERROR, "%d can not set camera buffer.", cameraNum);
        return DEF_RET_NG;
    }

    if (map_buffer(CameraInfo->fd, CameraInfo->buffer_size, CameraInfo->buffers) == DEF_RET_NG)
    {
        dprintf(ERROR, "%d can not map camera buffer.", cameraNum);
        return DEF_RET_NG;
    }

    if (enqueue_buffers(CameraInfo->fd, CameraInfo->buffer_size) == DEF_RET_NG)
    {
        dprintf(ERROR, "%d can not enqueue buffers.", cameraNum);
        return DEF_RET_NG;
    }

    if (start_streaming(CameraInfo->fd) == DEF_RET_NG)
    {
        dprintf(ERROR, "%d can not start streaming.", cameraNum);
        return DEF_RET_NG;
    }
    com_timer_init(ENUM_TIMER_CAMERA, g_CameraInfo->period);
    g_CameraStat[cameraNum]->Stat = 0;
    com_shmem_write(CameraInfo->shm_id, g_CameraStat[cameraNum], sizeof(cameraStat));

    while (gComm_StopFlg == DEF_COMM_OFF)
    {
        int index = dequeue_buffer(CameraInfo->fd);
        if (index == -1)
        {
            if(timeout_cnt > g_CameraInfo->timeout)
            {
                g_CameraStat[cameraNum]->Stat = 1;
                com_shmem_write(CameraInfo->shm_id, g_CameraStat[cameraNum], sizeof(cameraStat));
            }

			com_mtimer(ENUM_TIMER_CAMERA);
            timeout_cnt += g_CameraInfo->period;
            
            continue;
        }
        
        timeout_cnt = 0;
        g_CameraStat[cameraNum]->Stat = 0;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        g_CameraStat[cameraNum]->timestamp = now.tv_sec * 1000 + now.tv_nsec / 1000000;

        memcpy(g_CameraStat[cameraNum]->img_data , CameraInfo->buffers[index].start, CameraInfo->buffers[index].length);
        com_shmem_write(CameraInfo->shm_id, g_CameraStat[cameraNum], sizeof(cameraStat));
        //free(pCameraStat);

        enqueue_buffer(CameraInfo->fd, index);

        com_mtimer(ENUM_TIMER_CAMERA);
        timeout_cnt += g_CameraInfo->period;
    }

    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   デバイスオープン処理
 * @note    ファイルディスクリプタ取得．CameraInfoのfdを更新する．
 * @param   引数  : cameraInfo  *CameraInfo
 *                  
 * @return  戻り値: int ファイルディスクリプタ
 * @date    2023/12/08 [0.0.1] 新規作成
 */
/*============================================================================*/
static int OpenDevice(cameraInfo *CameraInfo)
{
    CameraInfo->fd = open(CameraInfo->dev_name, O_RDWR);
    if(CameraInfo->fd == -1)
    {
        dprintf(ERROR, "failed to open. device = %s\n", CameraInfo->dev_name);
    }
    return CameraInfo->fd;
}

/*============================================================================*/
/*
 * @brief   カメラ開始処理
 * @note    指定された番号のカメラ開始
 * @param   引数  : size_t  cameraNum   カメラの番号
 *                  
 * @return  戻り値: int 0:成功，-1:失敗
 * @date    2023/12/08 [0.0.1] 新規作成
 */
/*============================================================================*/
static int CameraStart(size_t cameraNum)
{
    dprintf(INFO, "CameraStart(%d)\n", cameraNum);

    int ret;
    int fd;

    cameraInfo *CameraInfo = &g_CameraInfo[cameraNum];

    fd = OpenDevice(CameraInfo);    
    if(fd == -1)
    {
        dprintf(ERROR, "camera%d, fail to OpenDevice(),  error=%d\n", cameraNum, errno);      
        return DEF_RET_NG;
    }

    ret = CameraRun(CameraInfo, cameraNum);
    if(ret == DEF_RET_NG)
    {
        close_device(CameraInfo->fd);
        dprintf(ERROR, "camera%d, fail to CameraRun(), error=%d\n", cameraNum, errno);
        return DEF_RET_NG;
    }

    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   カメラ終了処理
 * @note    カメラ終了処理
 * @param   引数  : cameraInfo  *CameraInfo
 * @param           size_t  camera_num
 *                  
 * @return  戻り値: void
 * @date    2023/12/08 [0.0.1] 新規作成
 */
/*============================================================================*/
static void CameraEnd(size_t cameraNum)
{
    dprintf(INFO, "CameraEnd(%d)\n", cameraNum);

    cameraInfo *CameraInfo = &g_CameraInfo[cameraNum];
    stream_stop(CameraInfo->fd);
    close_device(CameraInfo->fd);
    com_shmem_close(CameraInfo->shm_id);
    unmap_buffer(CameraInfo->buffer_size, CameraInfo->buffers);
    //free(CameraInfo.buffers);
}

/*============================================================================*/
/*
 * @brief   カメラ初期化処理
 * @note    画素数，バッファサイズを設定．CameraMainスレッドにCPUアフィニティを設定．
 * @param   引数  : cameraInfo  *CameraInfo
 * @param           size_t  camera_num
 *                  
 * @return  戻り値: void
 * @date    2023/12/08 [0.0.1] 新規作成
 */
/*============================================================================*/
static int CameraInit(size_t cameraNum)
{
    dprintf(INFO, "CameraInit(%d)\n", cameraNum);
    
    char shm_name[64];

    cameraInfo *CameraInfo = &g_CameraInfo[cameraNum];
    CameraInfo->camera_num = cameraNum;
    CameraInfo->img_height = DEF_IMG_HEIGHT;
    CameraInfo->img_width = DEF_IMG_WIDTH;
    CameraInfo->buffer_size = DEF_BUFF_SIZE;
    //CameraInfo.buffers = (struct buffer *)malloc(CameraInfo.buffer_size * sizeof(CameraInfo.buffers));
    sprintf(shm_name, "%s%ld", "/readcam", CameraInfo->camera_num);

    CameraInfo->shm_id = com_shmem_open(shm_name, SHM_KIND_PLATFORM);
    if(CameraInfo->shm_id == DEF_COM_SHMEM_FALSE)
    {
        dprintf(WARN, "com_shmem_open error. name = %s\n", shm_name);
        return DEF_RET_NG;
    }

    setuid(0);
    cpu_set_t cpu_set;

    CPU_ZERO(&cpu_set);
    CPU_SET(cameraNum, &cpu_set);
    if(sched_setaffinity(gettid(), sizeof(cpu_set_t), &cpu_set) == -1)
    {
        dprintf(ERROR, "camera[%d] cpu set failed.\n", cameraNum);
        com_shmem_close(CameraInfo->shm_id);
        return DEF_RET_NG;
    }

    return DEF_RET_OK;
}

/*============================================================================*/
/*
 * @brief   カメラメイン処理
 * @note    カメラメイン処理
 * @param   引数  : void*		arg
 * @return  戻り値: void
 * @date    2023/12/08 [0.0.1] 新規作成
 */
/*============================================================================*/
void* CameraMain(void* arg)
{
    int ret;
    size_t cameraNum;
    resCameraInfo *ResCameraInfo = (resCameraInfo*)arg;

    cameraNum = ResCameraInfo->cameranum;
    strncpy(g_CameraInfo[cameraNum].dev_name, ResCameraInfo->devname, sizeof(g_CameraInfo[cameraNum].dev_name));
    g_CameraInfo[cameraNum].period = ResCameraInfo->period;
    g_CameraInfo[cameraNum].timeout = ResCameraInfo->timeout;

    g_CameraStat[cameraNum] = malloc(DEF_IMG_HEIGHT * DEF_IMG_WIDTH * 2);   /* メモリを割りあてる */
    if (NULL == g_CameraStat[cameraNum]) {
        pthread_exit(NULL);
    }
    ret = CameraInit(cameraNum);
    if(ret == DEF_RET_NG)
    {
        free(g_CameraStat[cameraNum]);  /* 割り当てられたメモリを解放する */
        pthread_exit(NULL);
    }

    for(int cnt = 0; cnt < DEF_WAIT_SECOND; cnt++)
    {
        ret = CameraStart(cameraNum);
        sleep(1);

        if(ret == DEF_RET_OK)
        {
            break;
        }
    }
    
    if(ret == DEF_RET_NG)
    {
        g_CameraStat[cameraNum]->Stat = 1;
        com_shmem_write(g_CameraInfo[cameraNum].shm_id, g_CameraStat[cameraNum], sizeof(cameraStat));

        com_shmem_close(g_CameraInfo[cameraNum].shm_id);
        free(g_CameraStat[cameraNum]);
        pthread_exit(NULL);
    }

    CameraEnd(cameraNum);

    pthread_exit(NULL);
}