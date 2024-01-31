#include <stdio.h>
#include <string.h>
#include "com_shmem.h"
#include "resource.h"

//書き込み用サンプル構造体
typedef struct _structSample{
    int i_sample;
    char c_sample[128];
} structSample;

void read_sample()
{
    int id;
    resourceStat ResourceStat;

    //共有メモリオープン
    id = com_shmem_open("/resstat", SHM_KIND_USER);

    //共有メモリ読込
	com_shmem_read(id, &ResourceStat, sizeof(ResourceStat));

    for(int i = 0; i < 13; i++)
	{
		printf("cpu_load[%d] = %d\n", i, ResourceStat.cpu_load[i]);
	}
	printf("mem_load = %d\n", ResourceStat.mem_load);
	printf("disk_load = %d\n", ResourceStat.disk_load);
	printf("cpu_therm = %d\n", ResourceStat.cpu_therm);

    //共有メモリクローズ
    com_shmem_close(id);
}

void write_sample()
{
    int id;
    structSample StructSample;

    //書き込み値を設定
    StructSample.i_sample = 1;
    strcpy(StructSample.c_sample, "sample");

    //共有メモリオープン
    id = com_shmem_open("/sample", SHM_KIND_USER);

    //共有メモリに書き込み
	com_shmem_write(id, &StructSample, sizeof(StructSample));

    //共有メモリクローズ
    com_shmem_close(id);
}

int main(void)
{
    //設定ファイル読込
    com_shmem_conf("../hjpf/memory.conf");

    read_sample();
    write_sample();
    
    return 0;
}