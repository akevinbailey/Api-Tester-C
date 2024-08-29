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

# Change libcurl.dll.a to the appropriate libcurl link library for the OS and CPU architecture.
# For MacOS:  brew install curl
# For Windows Visual Studio:  vcpkg.exe install curl:x64-windows
# For Windows MSYS2 gcc:  pacman -Sy mingw-w64-x86_64-toolchain
#                         pacman -Sy mingw-w64-x86_64-curl-winssl
# For Ubuntu:  sudo apt install build-essential
#              sudo apt install libcurl4-openssl-dev
# For Red Hat:  sudo dnf groupinstall "Development Tools"
#               sudo dnf install libcurl-devel
target_link_libraries(api-tester ${CMAKE_CURRENT_SOURCE_DIR}/libs/libcurl.dll.a)