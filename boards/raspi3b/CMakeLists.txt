
set (QEMU_FLAGS "-M raspi3b -nographic -s -kernel ${CMAKE_CURRENT_BINARY_DIR}/kernel-raspi3b.elf")
set (QEMU_FLAGS "${QEMU_FLAGS} -dtb ${CMAKE_CURRENT_SOURCE_DIR}/bcm2837-rpi-3-b.dtb")
set (QEMU_FLAGS "${QEMU_FLAGS} -drive file=${CMAKE_CURRENT_BINARY_DIR}/disk_image.img,id=disk0,if=none")

create_board_kernel(raspi3b ${CMAKE_CURRENT_SOURCE_DIR}/raspi3b.ld "board_conf.c;board_start.s")
create_diskimg(raspi3b)
create_qemu_target(raspi3b ${QEMU_FLAGS})

