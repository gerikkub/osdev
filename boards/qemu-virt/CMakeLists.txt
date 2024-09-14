
set (QEMU_FLAGS "-M virt,gic-version=3 -cpu cortex-a57 -nographic -s -kernel ${CMAKE_CURRENT_BINARY_DIR}/kernel-qemu-virt.elf")
set (QEMU_FLAGS "${QEMU_FLAGS} -drive file=${CMAKE_CURRENT_BINARY_DIR}/disk_image.img,id=disk0,if=none")
set (QEMU_FLAGS "${QEMU_FLAGS} -device virtio-blk-pci,drive=disk0,disable-legacy=on")
set (QEMU_FLAGS "${QEMU_FLAGS} -device virtio-serial-pci,disable-legacy=on")
set (QEMU_FLAGS "${QEMU_FLAGS} -chardev stdio,id=virtiocon0 -device virtconsole,chardev=virtiocon0")
#set (QEMU_FLAGS "${QEMU_FLAGS} -netdev tap,id=net0")
#set (QEMU_FLAGS "${QEMU_FLAGS} -netdev user,id=net0")
#set (QEMU_FLAGS "${QEMU_FLAGS} -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56")
set (QEMU_FLAGS "${QEMU_FLAGS} -monitor none -serial none")

create_board_kernel(qemu-virt ${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld "board_conf.c;board_start.s;board_vmem_bootstrap.c")
create_diskimg(qemu-virt)
create_qemu_target(qemu-virt ${QEMU_FLAGS})

