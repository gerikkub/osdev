#!/bin/bash

args=("$@")

elf=${args[0]}
cmd="gdb-multiarch -batch -nx -ex 'file ${elf}' "

for pc in ${args[@]:1};
do
    cmd="${cmd} -ex 'x/1i 0x${pc}'"
done

echo ${cmd}
eval ${cmd}