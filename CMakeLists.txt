cmake_minimum_required(VERSION 3.29)
project(api-tester)

set(
        CMAKE_CXX_STANDARD 20
        CURL_USE_STATIC_LIBS TRUE
)

find_package(CURL REQUIRED)

add_executable(api-tester
        api-tester.c
        api-tester.h)

target_link_libraries(api-tester ${CMAKE_CURRENT_SOURCE_DIR}/libs/libcurl.dll.a)