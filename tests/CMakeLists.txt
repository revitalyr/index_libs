cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
set(CMAKE_CXX_FLAGS '-stdlib=libc++')

set(CMAKE_CXX_MODULE_STD 1)

project(bfd_test LANGUAGES CXX)

cmake_policy(SET CMP0179 NEW)

add_library(bfd_test_lib STATIC)
target_sources(bfd_test_lib
  PUBLIC FILE_SET CXX_MODULES FILES   
    bfd_test.cxx)

# Tell CMake we're using C++23 but only C++20 is needed to consume it.
target_compile_features(bfd_test_lib
  PRIVATE   cxx_std_23
  INTERFACE cxx_std_20)

add_executable(main)
target_sources(main
  PRIVATE
    bfd_test.cpp)
target_link_libraries(main PRIVATE bfd_test_lib bfd)
