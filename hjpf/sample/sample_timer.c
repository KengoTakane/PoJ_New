#include <stdio.h>
#include "com_shmem.h"
#include "com_timer.h"

int main(void)
{
    //設定ファイルの読込
    com_shmem_conf("../hjpf/memory.conf");

    //タイマ初期設定
    com_timer_init(10, 100);

    while(1)
    {
        //処理
        printf("sample\n");

        //スリープ
        com_mtimer(10);
    }
    return 0;
}