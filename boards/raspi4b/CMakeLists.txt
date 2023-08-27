
set (QEMU_FLAGS "-M raspi4b -nographic -s -kernel ${CMAKE_CURRENT_BINARY_DIR}/kernel-raspi4b.elf")
set (QEMU_FLAGS "${QEMU_FLAGS} -dtb ${CMAKE_CURRENT_SOURCE_DIR}/bcm2837-rpi-3-b.dtb")
set (QEMU_FLAGS "${QEMU_FLAGS} -drive file=${CMAKE_CURRENT_BINARY_DIR}/disk_image.img,id=disk0,if=none")

create_board_kernel(raspi4b ${CMAKE_CURRENT_SOURCE_DIR}/raspi4b.ld "board_conf.c;board_start.s")
create_diskimg(raspi4b)
create_qemu_target(raspi4b ${QEMU_FLAGS})

add_custom_command(OUTPUT kernel-raspi4b.bin
                   COMMAND aarch64-linux-gnu-objcopy -O binary kernel-raspi4b.elf kernel-raspi4b.bin
                   DEPENDS kernel-raspi4b
                   VERBATIM
)

add_custom_target(load-raspi4b
                  COMMAND cp kernel-raspi4b.bin /mnt/c/TFTP-Root/b55dbdaa/kernel8.img
                  DEPENDS kernel-raspi4b.bin
                  VERBATIM)
