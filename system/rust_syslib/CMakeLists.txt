
file(GLOB RUST_SOURCES "src/*.rs")
set(APPEND RUST_SOURCES Cargo.toml)
set(APPEND RUST_SOURCES build.rs)

add_custom_command(OUTPUT build/target/debug/librust_syslib.a
    COMMAND ${CMAKE_COMMAND} -E env CROSS_DIR=${COMPILER_PATH} cargo build
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${RUST_SOURCES})

add_library(rust_syslib INTERFACE build/target/debug/librust_syslib.a)


