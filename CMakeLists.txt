
cmake_minimum_required (VERSION 3.0.0)

set (CMAKE_SYSTEM_NAME Generic)
set (CMAKE_SYSTEM_PROCESSOR aarch64)

project (osdev_kernel C ASM)

set (KERNEL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}
                         ${CMAKE_SOURCE_DIR}/include
                         ${CMAKE_SOURCE_DIR}/stdlib
                         ${CMAKE_SOURCE_DIR}/stdlib/minimal/include)

#set (CMAKE_ASM_FLAGS "-x assembler-with-cpp -Wall -fdata-sections -ffunction-sections")
set (CMAKE_ASM_FLAGS "-x assembler-with-cpp")
set (CMAKE_C_FLAGS "-ffreestanding -Wall -Werror -mno-pc-relative-literal-loads")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mgeneral-regs-only -march=armv8.2-a")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -g -gdwarf-2 -Og")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set (KERNEL_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/kernel.ld")
set (CMAKE_EXE_LINKER_FLAGS "-ffreestanding -nostdlib -Wl,-z -Wl,max-page-size=4096")

add_subdirectory (drivers)
add_subdirectory (kernel)
add_subdirectory (stdlib)
add_subdirectory (bootstrap)

add_subdirectory (system)


# add_executable(kernel)

# set_target_properties(kernel PROPERTIES OUTPUT_NAME "kernel.elf"
#                                         LINK_FLAGS "-Wl,-T${KERNEL_LINKER_SCRIPT} -Wl,-Map=kernel.map")
# target_include_directories(kernel PRIVATE ${KERNEL_INCLUDE_DIRS})
# target_compile_options(kernel PRIVATE "-DKERNEL_BUILD")

# get_target_property(KERNEL_BIN kernel OUTPUT_NAME)


# target_link_libraries(kernel
#                       kernel_drivers
#                       kernel_src
#                       kernel_stdlib
#                       kernel_bootstrap)

# include_directories(${CMAKE_SOURCE_DIR})


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


function(create_diskimg NAME)
    set (GENEXT2FS_BIN /home/gerik/projects/osdev-monolithic/tools/genext2fs/bin/genext2fs)

    add_custom_target (diskimg-${NAME}
        COMMAND rm -rf diskdata
        COMMAND mkdir -p diskdata/bin
        COMMAND cp ${APP_BINARIES} diskdata/bin
        COMMAND cp -r ${CMAKE_SOURCE_DIR}/diskextras/* diskdata/
        COMMAND ${GENEXT2FS_BIN} -b 2880 -d diskdata/ ${CMAKE_CURRENT_BINARY_DIR}/disk_image.img
        DEPENDS kernel-${NAME} ${APP_TARGETS})
    set_target_properties(diskimg-${NAME} PROPERTIES OUTPUT_NUM "disk_image.img")
endfunction()

set (QEMU_PATH /home/gerik/Downloads/qemu/build/)

function(create_qemu_target NAME QEMU_FLAGS)

    string (REPLACE " " ";" QEMU_VM_FLAGS "${QEMU_FLAGS}")
    string (REPLACE " " ";" QEMU_VM_DEBUG_FLAGS "${QEMU_FLAGS} -S")

    message(STATUS "QEMU Args: ${QEMU_VM_FLAGS}")

    add_custom_target(run-${NAME}
        COMMAND ${QEMU_PATH}/qemu-system-aarch64 ${QEMU_VM_FLAGS}
                    DEPENDS diskimg-${NAME})

    add_custom_target(debug-${NAME}
        COMMAND ${QEMU_PATH}/qemu-system-aarch64 ${QEMU_VM_DEBUG_FLAGS}
                    DEPENDS diskimg-${NAME})

endfunction()

add_subdirectory (boards)



#set (QEMU_VM_FLAGS -M raspi3b -nographic -s -kernel ${PROJECT_BINARY_DIR}/kernel.elf)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -drive file=${CMAKE_BINARY_DIR}/disk_image.img,id=disk0,if=none)

#set (QEMU_VM_FLAGS -M virt,gic-version=3 -cpu cortex-a57 -nographic -s -kernel ${PROJECT_BINARY_DIR}/kernel.elf)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -device virtio-blk-pci,drive=disk0,disable-legacy=on)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -device virtio-serial-pci,disable-legacy=on)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -chardev stdio,id=virtiocon0 -device virtconsole,chardev=virtiocon0)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -netdev tap,id=net0)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -monitor none -serial none)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} -smp 2)
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} --trace "gicv3_dist_set_irq")
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} --trace "gicv3_cpuif_update")
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} --trace "gicv3_cpuif_set_irqs")
#set (QEMU_VM_FLAGS ${QEMU_VM_FLAGS} --trace "gicv3_*")

#set (QEMU_VM_DEBUG_FLAGS ${QEMU_VM_FLAGS} -S)

#set (QEMU_VM_DTB_FLAGS ${QEMU_VM_FLAGS} -machine dumpdtb=dtb.dtb)

#set (QEMU_PATH /usr/local/share/qemu/bin)

# add_custom_target (run
#     COMMAND ${QEMU_PATH}/qemu-system-aarch64 ${QEMU_VM_FLAGS}
#                    DEPENDS disk_image)

# add_custom_target (debug
#     COMMAND ${QEMU_PATH}/qemu-system-aarch64 ${QEMU_VM_DEBUG_FLAGS}
#                    DEPENDS disk_image)

# add_custom_target (dtb.dtb
#     COMMAND ${QEMU_PATH}/qemu-system-aarch64 ${QEMU_VM_DTB_FLAGS}
#                    DEPENDS disk_image)
                
                   
# add_custom_target (dts.txt
#     COMMAND dtc -I dtb -O dts -o dts.txt dtb.dtb
#                    DEPENDS dtb.dtb)

