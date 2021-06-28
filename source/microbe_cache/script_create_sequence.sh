#!/bin/bash

sizes=(
    # 4096
    # 8192
    # 16384
    # 32768
    # 65536
    # 131072
    # 160100
    # 524288
    # 1048576
    # 2097152
    # 4194304
    # 16777216
    33554432
    # 67108864
    # 134217728
    # 268435456
    # 402653184
)

for size in ${sizes[@]}
do
    (
	echo `pwd`
	mkdir -p benchmark_install/input/sequence_${size}
	cd benchmark_install/input/sequence_${size}
	../../bin/create_sequence ${size}
    )
done