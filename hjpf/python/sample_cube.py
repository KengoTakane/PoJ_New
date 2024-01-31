import comshmem
import sys
import time

def cube_read_sample():
    #共有メモリオープン
    id = shm.open('/mavlink_recv', comshmem.ShmemKind.PLATFORM)
    #共有メモリ読込
    bytes = shm.read()

    #キューブパイロット受信用のインスタンス生成
    cube_recv = comshmem.Cube_Recv()

    #バイト列から値に変換
    cube_recv.fromByte(bytes)

    #各値の出力
    print('stat = ', cube_recv.Stat)
    print('time_unix_usec = ', cube_recv.time_unix_usec)
    print('time_boot_ms = ', cube_recv.time_boot_ms)

    #共有メモリクローズ
    shm.close()

def cube_write_sample():
    #共有メモリオープン
    ret = shm.open('/mavlink_send', comshmem.ShmemKind.USER)

    #キューブパイロット送信用のインスタンス生成
    cube_send = comshmem.Cube_Send()

    #書き込む値をセット
    cube_send.timestamp = time.time()
    cube_send.vx = 1.2333
    cube_send.vy = 2.0
    cube_send.vz = 100.5
    cube_send.yaw_rate = 1.0

    #バイト列に変換
    byte = cube_send.toByte()

    #共有メモリに書き込み
    shm.write(byte)

    #書き込んだ値を出力
    print("write = ", shm.read())
    
    #共有メモリクローズ
    shm.close()

if __name__ == '__main__':
    # 共有メモリのインスタンス生成
    shm = comshmem.Shmem('../hjpf/memory.conf')

    cube_write_sample()
    cube_read_sample()