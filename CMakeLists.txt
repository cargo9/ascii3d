cmake_minimum_required(VERSION 3.20)
project(ascii3d CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(ascii3d
  src/main.cpp
  src/term.cpp
  src/image.cpp
  src/mesh.cpp
  src/viewer3d.cpp
  src/imageview.cpp
  src/fontzoom.cpp
  src/third_party_impl.cpp
)
# SYSTEM — чтобы warning-и чужих библиотек не засоряли вывод
target_include_directories(ascii3d SYSTEM PRIVATE third_party)
target_compile_options(ascii3d PRIVATE -Wall -Wextra -Wpedantic)
