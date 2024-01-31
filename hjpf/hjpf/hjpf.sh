#!/bin/bash
/usr/bin/echo 950000 > /sys/fs/cgroup/cpu/system.slice/cpu.rt_runtime_us
/usr/bin/echo 950000 > /sys/fs/cgroup/cpu,cpuacct/system.slice/hjpf.service/cpu.rt_runtime_us

ls ../update/[0-9][0-9]*.sh | awk '{printf("../update/%s\n", $1)}' | sh
ls ../update/ | awk '{printf("rm ../update/%s\n", $1)}' | sh

./hjpf >& /tmp/hjpf.log
