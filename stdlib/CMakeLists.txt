
add_library(kernel_stdlib OBJECT
            ${CMAKE_CURRENT_SOURCE_DIR}/bitalloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/bitutils.c
            ${CMAKE_CURRENT_SOURCE_DIR}/errno.c
            ${CMAKE_CURRENT_SOURCE_DIR}/linalloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/malloc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/printf.c
            ${CMAKE_CURRENT_SOURCE_DIR}/snprintf.c

            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/atoi.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/bsearch.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/exit.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/qsort.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/rand.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/strtol.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/strtoll.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/strtoul.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdlib/strtoull.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdout/fprintf.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdout/sprintf.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/stdout/stdout_console.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/string/strerror.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/string/string.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/string/strncasecmp.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/string/strspn.c
            ${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/string/strstr.c
            #${CMAKE_CURRENT_SOURCE_DIR}/minimal/source/time/gmtime.c
)
target_include_directories(kernel_stdlib PRIVATE ${KERNEL_INCLUDE_DIRS})