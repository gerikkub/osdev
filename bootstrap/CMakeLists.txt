
add_library(kernel_bootstrap OBJECT
            ${CMAKE_CURRENT_SOURCE_DIR}/main_bootstrap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/vmem_bootstrap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/start.s
)
target_include_directories(kernel_bootstrap PRIVATE ${KERNEL_INCLUDE_DIRS})
target_compile_options(kernel_bootstrap PRIVATE "-s")