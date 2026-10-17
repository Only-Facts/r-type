cmake_minimum_required(VERSION 3.16)

project(r-type LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# PACKAGE MANAGER (CPM, downloaded once at configure time)

set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

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

# LIB (SFML is downloaded and built by CPM, not taken from the system)

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

add_executable(r-type_server ${SRC_COMMON} ${SRC_SERVER})
add_executable(r-type_client ${SRC_COMMON} ${SRC_CLIENT})

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
  SFML::Graphics
  SFML::Window
  SFML::System
  SFML::Network
)
