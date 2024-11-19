cmake_minimum_required(VERSION 3.28)
project(bof-exec)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(bof-exec
  src/bof-exec.cpp
  src/beacon_api.cpp
  src/util.cpp
  include/bof-exec.hpp
  include/structs.hpp
  include/macro.hpp
  include/beacon_api.hpp
  include/util.hpp
)

target_include_directories(bof-exec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)