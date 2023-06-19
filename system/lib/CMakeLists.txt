
add_library(system_lib OBJECT
            ${CMAKE_CURRENT_SOURCE_DIR}/system_console.c
            ${CMAKE_CURRENT_SOURCE_DIR}/system_file.c
            ${CMAKE_CURRENT_SOURCE_DIR}/system_lib.c
            ${CMAKE_CURRENT_SOURCE_DIR}/system_malloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/system_msg.c
            ${CMAKE_CURRENT_SOURCE_DIR}/system_socket.c
            ${CMAKE_CURRENT_SOURCE_DIR}/system_entry.s
)