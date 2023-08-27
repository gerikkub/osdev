

add_library(kernel_drivers OBJECT
            aarch64/aarch64.c
            console/console_dev.c
            gicv3/gicv3.c
            gicv2/gicv2.c
            #gicv3/gicv3_its.c
            pcie/pcie.c
            virtio_pci_blk/virtio_pci_blk.c
            virtio_pci_console/virtio_pci_console.c
            virtio_pci_early_console/virtio_pci_early_console.c
            virtio_pci_net/virtio_pci_net.c
            pl011_uart.c
            qemu_fw_cfg.c
            timer/bcm2835_systemtimer.c
)
target_include_directories(kernel_drivers PRIVATE ${KERNEL_INCLUDE_DIRS})