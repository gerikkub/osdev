
set (APP_TARGETS)
set (APP_BINARIES)

function(create_system_target NAME SOURCES)
    add_executable(system_${NAME} ${SOURCES})
    set_target_properties(system_${NAME} PROPERTIES OUTPUT_NAME "${NAME}.elf"
                                                LINK_FLAGS "-Wl,-T${APP_LINKER_SCRIPT} -Wl,-Map=${NAME}.map")
    target_include_directories(system_${NAME} PRIVATE ${APP_INCLUDE_DIRS})

    target_link_libraries(system_${NAME}
                        kernel_stdlib
                        system_lib)
    
    set(APP_TARGETS "${APP_TARGETS};system_${NAME}" PARENT_SCOPE)
    set(APP_BINARIES "${APP_BINARIES};${CMAKE_CURRENT_BINARY_DIR}/${NAME}.elf" PARENT_SCOPE)

endfunction()

set (APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib
                      ${CMAKE_CURRENT_SOURCE_DIR}/../
                      ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set (APP_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/system.ld")

add_subdirectory (lib)

target_include_directories(system_lib PRIVATE ${APP_INCLUDE_DIRS})

create_system_target(cat "cat/cat.c")
create_system_target(addline "addline/addline.c")
create_system_target(echo "echo/echo.c")
create_system_target(gsh "gsh/gsh.c")
create_system_target(http_server "http_server/http_server.c")
create_system_target(tcp_listen "tcp_listen/tcp_listen.c")
create_system_target(tcp_test "tcp_test/tcp_test.c")
create_system_target(time "time/time.c")
create_system_target(touch "touch/touch.c")
create_system_target(udp_recv "udp_recv/udp_recv.c")
create_system_target(udp_test "udp_test/udp_test.c")


set(APP_TARGETS "${APP_TARGETS}" PARENT_SCOPE)
set(APP_BINARIES "${APP_BINARIES}" PARENT_SCOPE)