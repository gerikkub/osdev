
set (APP_TARGETS)
set (APP_BINARIES)

function(create_system_target NAME SOURCES)
    add_executable(system_${NAME} ${SOURCES})
    set_target_properties(system_${NAME} PROPERTIES OUTPUT_NAME "${NAME}.elf"
                                                LINK_FLAGS "-Wl,-T${APP_LINKER_SCRIPT} -Wl,-Map=${NAME}.map")
    target_include_directories(system_${NAME} PRIVATE ${APP_INCLUDE_DIRS})

    target_link_libraries(system_${NAME}
                        userspace_stdlib
                        system_lib)
    
    set(APP_TARGETS "${APP_TARGETS};system_${NAME}" PARENT_SCOPE)
    set(APP_BINARIES "${APP_BINARIES};${CMAKE_CURRENT_BINARY_DIR}/${NAME}.elf" PARENT_SCOPE)

    message(STATUS "Created system target: system_${NAME}")
endfunction()

function(create_system_target_rust NAME)

    file(GLOB RUST_SOURCES "${NAME}/src/*.rs")
    set(APPEND RUST_SOURCES "${NAME}/Cargo.toml")

    set(RUSTLIB_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}/target/aarch64-unknown-none/debug/lib${NAME}.a)

    add_custom_command(OUTPUT ${RUSTLIB_FILE}
        COMMAND ${CMAKE_COMMAND} -E env CROSS_DIR=${COMPILER_PATH} cargo build --target=aarch64-unknown-none
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}
        DEPENDS ${RUST_SOURCES})

    add_library(system_${NAME}_rustlib INTERFACE ${RUSTLIB_FILE})
    target_link_libraries(system_${NAME}_rustlib INTERFACE ${RUSTLIB_FILE})

    add_executable(system_${NAME})
    set_target_properties(system_${NAME} PROPERTIES OUTPUT_NAME "${NAME}.elf"
                                                LINK_FLAGS "-Wl,-T${APP_LINKER_SCRIPT} -Wl,-Map=${NAME}.map")
    target_include_directories(system_${NAME} PRIVATE ${APP_INCLUDE_DIRS})

    target_link_libraries(system_${NAME}
                          system_${NAME}_rustlib
                          userspace_stdlib
                          system_lib)
    
    set(APP_TARGETS "${APP_TARGETS};system_${NAME}" PARENT_SCOPE)
    set(APP_BINARIES "${APP_BINARIES};${CMAKE_CURRENT_BINARY_DIR}/${NAME}.elf" PARENT_SCOPE)

    message(STATUS "Created Rust system target: system_${NAME}")
endfunction()

set (APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib
                      ${CMAKE_CURRENT_SOURCE_DIR}/../
                      ${CMAKE_CURRENT_SOURCE_DIR}/../include
                      ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set (APP_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/system.ld")

add_subdirectory (lib)
#add_subdirectory (rust_syslib)

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
create_system_target(tcp_cat "tcp_cat/tcp_cat.c")
create_system_target(count "count/count.c")
create_system_target_rust(hello_rust)


set(APP_TARGETS "${APP_TARGETS}" PARENT_SCOPE)
set(APP_BINARIES "${APP_BINARIES}" PARENT_SCOPE)
