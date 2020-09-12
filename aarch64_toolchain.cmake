
set (CMAKE_SYSTEM_NAME Generic)
set (CMAKE_SYSTEM_PROCESSOR aarch64)

set (COMPILER_PATH "${CMAKE_SOURCE_DIR}/../tools/aarch64-none-elf/bin")

#CMAKE_FORCE_C_COMPILER(${COMPILER_PATH}/aarch64-none-elf-gcc GNU)
#set (CMAKE_C_COMPILER ${COMPILER_PATH}/aarch64-none-elf-gcc)
#set (CMAKE_ASM_COMPILER ${COMPILER_PATH}/aarch64-none-elf-gcc)
#set (CMAKE_C_LINK_EXECUTABLE ${COMPILER_PATH}/aarch64-none-elf-ld)

set (CMAKE_C_COMPILER aarch64-linux-gnu-gcc-10)
set (CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc-10)
set (CMAKE_C_LINK_EXECUTABLE aarch64-linux-gnu-ld)

