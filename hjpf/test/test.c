#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "gnss.h"
#include "camera.h"
#include "com_fs.h"

int main(int argc, char *argv[])
{
	//failsafeInfo FailsafeInfo;
	//printf("failsafeInfo size = [%ld]\n", sizeof(FailsafeInfo));

	//unsigned long long test;
	//printf("unsigned long long size = [%ld]\n", sizeof(test));

	char	arg[256];

	printf("%s start\n", argv[0]);
	while(1) {
		arg[0] = '\0';
		for (int cnt = 1; cnt < argc; cnt++) {
			strcat(arg, argv[cnt]);
			strcat(arg, " ");
		}
		printf("arg=[%s]\n", arg);
		sleep(5);
	}
}
