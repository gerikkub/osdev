

add_library(kernel_src OBJECT
            ${CMAKE_CURRENT_SOURCE_DIR}/main.c
            ${CMAKE_CURRENT_SOURCE_DIR}/console.c
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers.c
            ${CMAKE_CURRENT_SOURCE_DIR}/dtb.c
            ${CMAKE_CURRENT_SOURCE_DIR}/elf.c
            ${CMAKE_CURRENT_SOURCE_DIR}/exception.c
            ${CMAKE_CURRENT_SOURCE_DIR}/exception_handler_table.c
            ${CMAKE_CURRENT_SOURCE_DIR}/exec.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fd.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs_manager.c
            ${CMAKE_CURRENT_SOURCE_DIR}/gtimer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/interrupt/interrupt.c
            ${CMAKE_CURRENT_SOURCE_DIR}/kernelspace.c
            ${CMAKE_CURRENT_SOURCE_DIR}/kmalloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/memoryspace.c
            ${CMAKE_CURRENT_SOURCE_DIR}/messages.c
            ${CMAKE_CURRENT_SOURCE_DIR}/modules.c
            ${CMAKE_CURRENT_SOURCE_DIR}/pagefault.c
            ${CMAKE_CURRENT_SOURCE_DIR}/panic.c
            ${CMAKE_CURRENT_SOURCE_DIR}/select.c
            ${CMAKE_CURRENT_SOURCE_DIR}/syscall.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sys_device.c
            ${CMAKE_CURRENT_SOURCE_DIR}/task.c
            ${CMAKE_CURRENT_SOURCE_DIR}/taskctrl.c
            ${CMAKE_CURRENT_SOURCE_DIR}/time.c
            ${CMAKE_CURRENT_SOURCE_DIR}/vfs.c
            ${CMAKE_CURRENT_SOURCE_DIR}/vmem.c

            ${CMAKE_CURRENT_SOURCE_DIR}/exception_asm.s
            ${CMAKE_CURRENT_SOURCE_DIR}/high_start.s
            ${CMAKE_CURRENT_SOURCE_DIR}/vmem_asm.s
            ${CMAKE_CURRENT_SOURCE_DIR}/cache.s

            ${CMAKE_CURRENT_SOURCE_DIR}/fs/ext2/ext2.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/ext2/ext2_helpers.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/file.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/sysfs/sysfs.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/sysfs/sysfs_task.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/sysfs/sysfs_time.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/sysfs/sysfs_vmalloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fs/sysfs/sysfs_profile.c

            ${CMAKE_CURRENT_SOURCE_DIR}/lib/circbuffer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/hashmap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/intmap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/libdtb.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/libpci.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/libvirtio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/llist.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/vmalloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/elapsedtimer.c

            ${CMAKE_CURRENT_SOURCE_DIR}/lock/lock.c
            ${CMAKE_CURRENT_SOURCE_DIR}/lock/mutex.c

            ${CMAKE_CURRENT_SOURCE_DIR}/net/arp.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/arp_table.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/ethernet.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/ipv4.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/ipv4_icmp.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/ipv4_route.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/net.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/net_api.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/tcp.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/tcp_bind.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/tcp_conn.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/tcp_socket.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/udp.c
            ${CMAKE_CURRENT_SOURCE_DIR}/net/udp_socket.c
            )
target_include_directories(kernel_src PRIVATE ${KERNEL_INCLUDE_DIRS})