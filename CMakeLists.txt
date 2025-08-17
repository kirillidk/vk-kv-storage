cmake_minimum_required(VERSION 3.17)
project(vk-kv-storage VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KV_STORAGE_LIB "${CMAKE_PROJECT_NAME}_lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost REQUIRED COMPONENTS container)

enable_testing()

add_subdirectory(src)
add_subdirectory(unit)