
cmake_minimum_required (VERSION 3.0.0)

set (CMAKE_SYSTEM_NAME Generic)

project (osdev_tests C ASM)

set(CMAKE_C_FLAGS "-g3")

include_directories("${CMAKE_CURRENT_LIST_DIR}/..")
include_directories("${CMAKE_CURRENT_LIST_DIR}/../stdlib")

add_executable(lstruct_test lstruct_test.c test_helpers.c ../kernel/lib/lstruct.c)