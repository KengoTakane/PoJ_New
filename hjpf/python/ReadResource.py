import comshmem
import sys
import datetime
import time
import csv

def read_sample():
    tmp = []

    #共有メモリオープン
    shm.open('/resstat', comshmem.ShmemKind.PLATFORM)

    #共有メモリ読込
    bytes = shm.read()

    #基本リソースのインスタンス生成
    resstat = comshmem.ResStat()

    #バイト列から値に変換
    resstat.fromByte(bytes)

    #時刻を保存
    now = datetime.datetime.now()
    tmp.append(now.strftime('%Y/%m/%d %H:%M:%S'))

    #各値を保存
    for cnt in range(resstat.cpu_num + 1):
        tmp.append(resstat.cpu_load[cnt])
    tmp.append(resstat.mem_load)
    tmp.append(resstat.disk_load)
    tmp.append(resstat.cpu_therm)
    
    #共有メモリクローズ
    shm.close()

    #フェールセーフ
    shm.open('/failsafeinfo', comshmem.ShmemKind.PLATFORM)
    bytes = shm.read()
    fsinfo = comshmem.FailsafeInfo()
    fsinfo.fromByte(bytes)
    for cnt in range(34):
        tmp.append(fsinfo.errcode[cnt])
    shm.close()

    with open('resource.csv', 'a', newline='', encoding='utf-8', errors='ignore') as f:
        writer = csv.writer(f, dialect='excel-tab', quoting=csv.QUOTE_ALL)
        writer.writerow(tmp)



if __name__ == '__main__':
    # 共有メモリのインスタンス生成
    shm = comshmem.Shmem('../hjpf/memory.conf')

    with open('resource.csv', 'w', newline='', encoding='utf-8', errors='ignore') as f:
        writer = csv.writer(f, dialect='excel-tab', quoting=csv.QUOTE_ALL)
        writer.writerow(['TIME', 'CPU', 'CPU1', 'CPU2', 'CPU3', 'CPU4', 'CPU5', 'CPU6', 'CPU7', 'CPU8', 'CPU9', 'CPU10', 'CPU11', 'CPU12', 'MEM', 'DISK', 'THERM'])

    while True:
        read_sample()
        time.sleep(1)
    
