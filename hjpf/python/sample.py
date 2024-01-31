import comshmem
import sys

def read_sample():
    #共有メモリオープン
    shm.open('/resstat', comshmem.ShmemKind.PLATFORM)

    #共有メモリ読込
    bytes = shm.read()

    #基本リソースのインスタンス生成
    resstat = comshmem.ResStat()

    #バイト列から値に変換
    resstat.fromByte(bytes)

    #各値の出力
    for cnt in range(resstat.cpu_num + 1):
        print('cpu_load[', cnt, '] = ', resstat.cpu_load[cnt])
    print('mem_load = ', resstat.mem_load)
    print('disk_load = ', resstat.disk_load)
    print('cpu_therm = ', resstat.cpu_therm)

    #値からバイト列に変換
    print(resstat.toByte())

    #共有メモリクローズ
    shm.close()

def write_sample():
    #共有メモリオープン
    ret = shm.open('/sample', comshmem.ShmemKind.USER)

    #書き込む値の設定
    i_sample = 1
    byte = i_sample.to_bytes(4, byteorder='little')
    
    #共有メモリに書き込み
    shm.write(byte)

    #書き込んだ値を出力
    print(shm.read())
    
    #共有メモリクローズ
    shm.close()

if __name__ == '__main__':
    # 共有メモリのインスタンス生成
    shm = comshmem.Shmem('../hjpf/memory.conf')

    read_sample()
    write_sample()