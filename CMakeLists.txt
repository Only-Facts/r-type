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

# CPM.cmake

set(CPM_DOWNLOAD_VERSION 0.40.2)
set(
  CPM_DOWNLOAD_LOCATION
  "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake"
)

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
  message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
    "${CPM_DOWNLOAD_LOCATION}"
    STATUS CPM_DOWNLOAD_STATUS
  )
  list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_ERROR_CODE)
  if(NOT CPM_DOWNLOAD_ERROR_CODE EQUAL 0)
    file(REMOVE "${CPM_DOWNLOAD_LOCATION}")
    message(FATAL_ERROR "Could not download CPM.cmake: ${CPM_DOWNLOAD_STATUS}")
  endif()
endif()

include("${CPM_DOWNLOAD_LOCATION}")

# SFML VIA CPM

CPMAddPackage(
  NAME SFML
  GITHUB_REPOSITORY SFML/SFML
  GIT_TAG 3.1.0
  GIT_SHALLOW ON
  OPTIONS
    "BUILD_SHARED_LIBS OFF"
    "SFML_USE_SYSTEM_DEPS OFF"
    "SFML_BUILD_AUDIO ON"
    "SFML_BUILD_EXAMPLES OFF"
    "SFML_BUILD_DOC OFF"
    "SFML_BUILD_TEST_SUITE OFF"
)

# FLAGS

add_compile_options(-Wall -Wextra -Werror -g)

# SRC

file(GLOB_RECURSE SRC_COMMON "src/common/*.cpp")
file(GLOB_RECURSE SRC_SERVER "src/server/*.cpp")
file(GLOB_RECURSE SRC_CLIENT "src/client/*.cpp")

# EXE

if(WIN32)
  if(UNIX)
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

# LINK

target_link_libraries(r-type_server PRIVATE
  SFML::System
  SFML::Network
)

target_link_libraries(r-type_client PRIVATE
  SFML::Audio
  SFML::Graphics
  SFML::Window
  SFML::System
  SFML::Network
)

if(WIN32)
  target_link_libraries(r-type_server PRIVATE crypt32 dnsapi ws2_32 iphlpapi)
  target_link_libraries(r-type_client PRIVATE crypt32 dnsapi ws2_32 iphlpapi)
endif()

add_custom_command(TARGET r-type_client POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:r-type_client>/assets)
