
cmake_minimum_required (VERSION 3.0.0)

include (CMakeForceCompiler)

#set (CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/aarch64_toolchain.cmake")

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

#set (CMAKE_CROSSCOMPILER 1)

project (kernel C ASM)

add_subdirectory (multiboot)
add_subdirectory (drivers)

set (CMAKE_C_FLAGS "-nostdinc -fno-builtin -fno-stack-protector -DPRINTF_INCLUDE_CONFIG_H -g")
set (CMAKE_EXE_LINKER_FLAGS "--script=${CMAKE_SOURCE_DIR}/multiboot/multiboot.ld -static --no-dynamic-linker --spec=nosys.specs")

add_executable(kernel
               multiboot/start.s
               kernel/main.c
               kernel/console.c
               kernel/kmalloc.c
               kernel/panic.c
               kernel/exception.s
               kernel/exception.c
               drivers/pl011_uart.c
              )

include_directories(${CMAKE_SOURCE_DIR})

set_target_properties(kernel PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/multiboot/multiboot.ld
    )

add_custom_target (run
    COMMAND qemu-system-aarch64 -M virt -cpu cortex-a72 -kernel ${kernel_BINARY_DIR}/kernel
                   DEPENDS kernel)


set_target_properties(kernel
PROPERTIES LINKER_LANGUAGE C
)
