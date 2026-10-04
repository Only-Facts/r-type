cmake_minimum_required(VERSION 3.16)

project(r-type LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# LIB CHECK

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

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

# LIB

target_link_libraries(r-type_client PRIVATE
  SFML::Graphics
  SFML::Window
  SFML::System
)
