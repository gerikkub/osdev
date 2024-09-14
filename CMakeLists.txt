
cmake_minimum_required (VERSION 3.0.0)

set (CMAKE_SYSTEM_NAME Generic)
set (CMAKE_SYSTEM_PROCESSOR aarch64)

project (osdev_kernel C ASM)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

set (KERNEL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}
                         ${CMAKE_SOURCE_DIR}/include
                         ${CMAKE_SOURCE_DIR}/stdlib
                         ${CMAKE_SOURCE_DIR}/stdlib/minimal/include)

#set (CMAKE_ASM_FLAGS "-x assembler-with-cpp -Wall -fdata-sections -ffunction-sections")
set (CMAKE_ASM_FLAGS "-x assembler-with-cpp")
set (CMAKE_C_FLAGS "-ffreestanding -Wall -Werror")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mgeneral-regs-only -march=armv8.2-a")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -g -gdwarf-2 -Og")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function -Wno-unused-but-set-variable")

set (KERNEL_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/kernel.ld")
set (CMAKE_EXE_LINKER_FLAGS "-ffreestanding -nostdlib -Wl,-z -Wl,max-page-size=4096")

add_subdirectory (drivers)
add_subdirectory (kernel)
add_subdirectory (stdlib)
add_subdirectory (bootstrap)

add_subdirectory (system)

function(create_board_kernel NAME LINKER_SCRIPT SOURCES)

    set (KERNEL_NAME kernel-${NAME})

    add_library(kernel_board_${NAME} OBJECT ${SOURCES})

    add_executable(${KERNEL_NAME})

    set_target_properties(${KERNEL_NAME} PROPERTIES OUTPUT_NAME "${KERNEL_NAME}.elf"
                                            LINK_FLAGS "-Wl,-T${LINKER_SCRIPT} -Wl,-Map=${KERNEL_NAME}.map")
    target_include_directories(${KERNEL_NAME} PRIVATE ${KERNEL_INCLUDE_DIRS})
    target_compile_options(${KERNEL_NAME} PRIVATE "-DKERNEL_BUILD")

    get_target_property(KERNEL_BIN ${KERNEL_NAME} OUTPUT_NAME)

    target_link_libraries(${KERNEL_NAME}
                          kernel_drivers
                          kernel_src
                          kernel_stdlib
                          kernel_bootstrap
                          kernel_board_${NAME})

    include_directories(${CMAKE_SOURCE_DIR})

    set_target_properties(${KERNEL_NAME}
    PROPERTIES LINKER_LANGUAGE C
    )

    message(STATUS "Create board target: ${KERNEL_NAME}")
endfunction()

find_program(GENEXT2FS_BIN genext2fs HINTS ${PROJECT_SOURCE_ROOT}/../tools/genext2fs/bin REQUIRED)

function(create_diskimg NAME)

    add_custom_target (diskimg-${NAME}
        COMMAND rm -rf diskdata
        COMMAND mkdir -p diskdata/bin
        COMMAND cp ${APP_BINARIES} diskdata/bin
        COMMAND cp -r ${CMAKE_SOURCE_DIR}/diskextras/* diskdata/
        COMMAND ${GENEXT2FS_BIN} -b 4320 -d diskdata/ ${CMAKE_CURRENT_BINARY_DIR}/disk_image.img
        DEPENDS kernel-${NAME} ${APP_TARGETS})
    set_target_properties(diskimg-${NAME} PROPERTIES OUTPUT_NUM "disk_image.img")
endfunction()

function(create_tarball NAME)
    add_custom_target (disktar-${NAME}
        COMMAND rm -rf diskdata
        COMMAND mkdir -p diskdata/bin
        COMMAND cp ${APP_BINARIES} diskdata/bin
        COMMAND cp -r ${CMAKE_SOURCE_DIR}/diskextras/* diskdata/
        COMMAND tar --no-acls --no-xattrs --no-selinux -H gnu -C diskdata -czf ${CMAKE_CURRENT_BINARY_DIR}/disk_image.tar .
        DEPENDS kernel-${NAME} ${APP_TARGETS})
    set_target_properties(disktar-${NAME} PROPERTIES OUTPUT_NUM "disk_image.tar")
endfunction()

find_program(QEMU_BIN qemu-system-aarch64 HINTS /home/gerik/Downloads/qemu/build /home/gerik/src/qemu/install/bin REQUIRED)

function(create_qemu_target NAME QEMU_FLAGS)

    string (REPLACE " " ";" QEMU_VM_FLAGS "${QEMU_FLAGS}")
    string (REPLACE " " ";" QEMU_VM_DEBUG_FLAGS "${QEMU_FLAGS} -S")

    add_custom_target(run-${NAME}
        COMMAND ${QEMU_BIN} ${QEMU_VM_FLAGS}
                    DEPENDS diskimg-${NAME})

    add_custom_target(debug-${NAME}
        COMMAND ${QEMU_BIN} ${QEMU_VM_DEBUG_FLAGS}
                    DEPENDS diskimg-${NAME})

endfunction()

add_subdirectory (boards)


# add_custom_target (dtb.dtb
#     COMMAND ${QEMU_PATH}/qemu-system-aarch64 ${QEMU_VM_DTB_FLAGS}
#                    DEPENDS disk_image)
                   
# add_custom_target (dts.txt
#     COMMAND dtc -I dtb -O dts -o dts.txt dtb.dtb
#                    DEPENDS dtb.dtb)

