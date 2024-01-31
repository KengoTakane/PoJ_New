import comshmem
import sys
import datetime
import time
import csv

def read_sample():
    tmp = []

    #共有メモリオープン
    shm.open('/procstat', comshmem.ShmemKind.PLATFORM)

    #共有メモリ読込
    bytes = shm.read()

    #基本リソース(プロセス)のインスタンス生成
    procstat = comshmem.ProcStat()

    #バイト列から値に変換
    procstat.fromByte(bytes)

    #時刻を保存
    now = datetime.datetime.now()
    tmp.append(now.strftime('%Y/%m/%d %H:%M:%S'))

    #各値を保存
    for cnt in range(procstat.num):
        tmp.append(procstat.cpu[cnt])
        tmp.append(procstat.mem[cnt])
    
    #共有メモリクローズ
    shm.close()

    with open('process_status.csv', 'a', newline='', encoding='utf-8', errors='ignore') as f:
        writer = csv.writer(f, quoting=csv.QUOTE_ALL)
        writer.writerow(tmp)

if __name__ == '__main__':
    # 共有メモリのインスタンス生成
    shm = comshmem.Shmem('../hjpf/memory.conf')

    #共有メモリオープン
    shm.open('/procstat', comshmem.ShmemKind.PLATFORM)

    #共有メモリ読込
    bytes = shm.read()

    #基本リソース(プロセス)のインスタンス生成
    procstat = comshmem.ProcStat()

    #バイト列から値に変換
    procstat.fromByte(bytes)

    #ヘッダー
    header = ['TIME']
    for cnt in range(procstat.num):
        header.append('PROC' + str(cnt) + 'CPU')
        header.append('PROC' + str(cnt) + 'MEM')

    shm.close()

    with open('process_status.csv', 'w', newline='', encoding='utf-8', errors='ignore') as f:
        writer = csv.writer(f, quoting=csv.QUOTE_ALL)
        writer.writerow(header)

    while True:
        read_sample()
        time.sleep(1)

