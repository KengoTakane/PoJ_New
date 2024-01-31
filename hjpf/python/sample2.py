import comshmem
import sys
import numpy as np
import cv2

def read_sample():
    #共有メモリオープン
    shm.open('/readcam5', comshmem.ShmemKind.PLATFORM)

    while(True):
        #共有メモリ読込
        bytes = shm.read()

        #基本リソースのインスタンス生成
        camera = comshmem.Camera()
        camera.fromByte(bytes)

        #src = np.frombuffer(camera.img_data, dtype=np.uint8).reshape(3840, 2160, 2)
        src = np.frombuffer(camera.img_data, dtype=np.uint8).reshape(2160, 3840, 2)
        dst = cv2.cvtColor(src, cv2.COLOR_YUV2BGR_UYVY)
        cv2.imshow('camera', dst)

        k = cv2.waitKey(1)
        if k == ord('q'):
            break

    #共有メモリクローズ
    shm.close()


if __name__ == '__main__':
    # 共有メモリのインスタンス生成
    shm = comshmem.Shmem('../hjpf/memory.conf')

    read_sample()
