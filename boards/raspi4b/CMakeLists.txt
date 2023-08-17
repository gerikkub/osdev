
set (QEMU_FLAGS "-M raspi4b -nographic -s -kernel ${CMAKE_CURRENT_BINARY_DIR}/kernel-raspi4b.elf")
set (QEMU_FLAGS "${QEMU_FLAGS} -dtb ${CMAKE_CURRENT_SOURCE_DIR}/bcm2837-rpi-3-b.dtb")
set (QEMU_FLAGS "${QEMU_FLAGS} -drive file=${CMAKE_CURRENT_BINARY_DIR}/disk_image.img,id=disk0,if=none")

create_board_kernel(raspi4b ${CMAKE_CURRENT_SOURCE_DIR}/raspi4b.ld "board_conf.c;board_start.s")
create_diskimg(raspi4b)
create_qemu_target(raspi4b ${QEMU_FLAGS})

