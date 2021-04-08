#!/bin/bash

CC="../llvm-project/build-shared/bin/clang"
CINVOC="$CC  --std=c2x -g -O2 -target riscv64-unknown-freebsd --sysroot=/home/dusanz/cheri/output/rootfs-riscv64-purecap -fuse-ld=/home/dusanz/cheri/build/llvm-project-build/bin/ld.lld -mno-relax -march=rv64gcxcheri -mabi=l64pc128d -Wall -Wcheri"

function build_w_runtime(){
	$CINVOC runtime.o -c $1.c -o $1.o
}

function build(){
    $CC --std=c2x -g -O2 -target riscv64-unknown-freebsd --sysroot=/home/dusanz/cheri/output/rootfs-riscv64-purecap -fuse-ld=/home/dusanz/cheri/build/llvm-project-build/bin/ld.lld -mno-relax -march=rv64gcxcheri -mabi=l64pc128d -Wall -Wcheri $1.c -o $1 ${@:2}

    $CC -S -emit-llvm --std=c2x -O0 -target riscv64-unknown-freebsd --sysroot=/home/dusanz/cheri/output/rootfs-riscv64-purecap -mno-relax -march=rv64gcxcheri -mabi=l64pc128d -Wall -Wcheri $1.c
    #$CC -cc1 -internal-isystem /home/shared/Documents/cambridge/Materjali_I_Domaci/III/Project/repos/llvm-project/build/lib/clang/11.0.0/include -nostdsysteminc -triple mips64-unknown-freebsd -target-cpu cheri128 -cheri-size 128 -mllvm -verify-machineinstrs -target-abi purecap -std=c2x -ast-dump $1.c > $1.ast
	#$CC -cc1 -internal-isystem /home/shared/Documents/cambridge/Materjali_I_Domaci/III/Project/repos/llvm-project/build/lib/clang/11.0.0/include -isystem=/home/dusanz/cheri/output/rootfs-riscv64-purecap/usr/include -nostdsysteminc -triple mips64-unknown-freebsd -target-cpu cheri128 -cheri-size 128 -mllvm -verify-machineinstrs -target-abi purecap -std=c2x -ast-dump $1.c -o $1.ast
}

function sync(){
    scp $* cheri:~/test/
}

$1 ${@:2}


