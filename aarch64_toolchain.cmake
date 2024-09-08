
set (CMAKE_SYSTEM_NAME Generic)
set (CMAKE_SYSTEM_PROCESSOR aarch64)

file (REAL_PATH "../tools/compiler/bin" COMPILER_PATH)

set (CMAKE_C_COMPILER_WORKS 1)

set (CMAKE_C_COMPILER ${COMPILER_PATH}/aarch64-none-elf-gcc)
set (CMAKE_ASM_COMPILER ${COMPILER_PATH}/aarch64-none-elf-gcc)
set (CMAKE_CROSSCOMPILING 1)

