cmake_minimum_required(VERSION 3.16)

# CROSS COMPILATION

option(WIN "Windows cross-compilation with MinGW" OFF)

if(WIN)
  set(
    CMAKE_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain/x86_64-w64-mingw32.cmake"
    CACHE FILEPATH "Toolchain file"
  )
endif()

# PROJECT

project(r-type LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# LIB CHECK

if(WIN)
  find_package(
    SFML 3.1 COMPONENTS Graphics Window System Network
    QUIET NO_CMAKE_SYSTEM_PATH
  )
else()
  find_package(SFML 3.1 COMPONENTS Graphics Window System Network QUIET)
endif()

if(NOT SFML_FOUND)
  include(FetchContent)
  message(STATUS "SFML 3.1 not found. Downloading & compiling from GitHub...")

  FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
    )

  set(SFML_USE_SYSTEM_DEPS OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_DOC OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(SFML)
endif()

# FLAGS

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Werror -g)
endif()

# SRC

file(GLOB_RECURSE SRC_COMMON "src/common/*.cpp")
file(GLOB_RECURSE SRC_SERVER "src/server/*.cpp")
file(GLOB_RECURSE SRC_CLIENT "src/client/*.cpp")

# EXE

if(WIN32)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "/usr/x86_64-w64-mingw32/lib/libcrypt32.a"
      "${CMAKE_BINARY_DIR}/libCrypt32.a"
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "/usr/x86_64-w64-mingw32/lib/libdnsapi.a"
      "${CMAKE_BINARY_DIR}/libDnsapi.a"
  )

  link_directories("${CMAKE_BINARY_DIR}")
endif()

add_executable(r-type_server ${SRC_COMMON} ${SRC_SERVER})
add_executable(r-type_client ${SRC_COMMON} ${SRC_CLIENT})

if(WIN32)
  set_target_properties(r-type_server PROPERTIES SUFFIX ".exe")
  set_target_properties(r-type_client PROPERTIES SUFFIX ".exe")

  target_link_options(r-type_server PRIVATE -static-libgcc -static-libstdc++)
  target_link_options(r-type_client PRIVATE -static-libgcc -static-libstdc++)
endif()

# INCLUDE

include_directories("include/common/")

target_include_directories(r-type_server PRIVATE "include/server/")
target_include_directories(r-type_client PRIVATE "include/client/")

# LIB

target_link_libraries(r-type_server PRIVATE
  SFML::System
  SFML::Network
)

target_link_libraries(r-type_client PRIVATE
  SFML::Graphics
  SFML::Window
  SFML::System
  SFML::Network
)

if(WIN32)
  target_link_libraries(r-type_server PRIVATE crypt32 dnsapi ws2_32 iphlpapi)
  target_link_libraries(r-type_client PRIVATE crypt32 dnsapi ws2_32 iphlpapi)
endif()
