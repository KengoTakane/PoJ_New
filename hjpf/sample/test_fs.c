#include <stdio.h>
#include "com_fs.h"
#include "com_shmem.h"

int main(void)
{
	com_shmem_conf("../hjpf/memory.conf");
	int value = -1;

	for (uint32_t i = 1; i <= 34; i++)
	{
		value = com_fs_getfail(i);
		printf("ErroCode = %d : fail level = %d\n", i, value);
	}

	return 0;
}
