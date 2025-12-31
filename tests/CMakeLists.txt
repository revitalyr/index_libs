cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

# Enable experimental import std support BEFORE project() for Clang/GCC (not needed for MSVC)
if(UNIX)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    if(NOT DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
      set(_search_paths
          "${CMAKE_ROOT}/Help/dev/experimental.rst"
          "${CMAKE_ROOT}/share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Help/dev/experimental.rst"
          "${CMAKE_ROOT}/../share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Help/dev/experimental.rst"
          "/usr/share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Help/dev/experimental.rst"
          "/usr/local/share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Help/dev/experimental.rst"
      )
      foreach(_path ${_search_paths})
        if(EXISTS "${_path}")
          file(READ "${_path}" _experimental_content)
          if(_experimental_content MATCHES "``([0-9a-f-]{36})``")
            set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "${CMAKE_MATCH_1}" CACHE STRING "Experimental C++23 import std UUID" FORCE)
            message(STATUS "Detected C++23 import std UUID: ${CMAKE_EXPERIMENTAL_CXX_IMPORT_STD}")
            break()
          endif()
        endif()
      endforeach()
    endif()
    if(NOT DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
      if(CMAKE_VERSION VERSION_LESS "3.31")
         set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
      else()
         message(FATAL_ERROR "Could not determine CMAKE_EXPERIMENTAL_CXX_IMPORT_STD UUID for CMake ${CMAKE_VERSION}.")
      endif()
    endif()
    set(CMAKE_CXX_MODULE_STD 1)
  elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
    set(CMAKE_CXX_MODULE_STD 1)
  endif()
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project(index_libs_tests LANGUAGES CXX C)

# Parent project root
get_filename_component(PARENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Clang on Unix: use libc++ and find std.cppm
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options($<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>)
    
    if(NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    endif()

    # Find clang-scan-deps
    if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
        get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        if(CMAKE_CXX_COMPILER_VERSION MATCHES "^([0-9]+)")
            set(_clang_major "${CMAKE_MATCH_1}")
            find_program(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS 
                         NAMES clang-scan-deps-${_clang_major} clang-scan-deps
                         HINTS "${_compiler_dir}" "/usr/lib/llvm-${_clang_major}/bin")
        endif()
    endif()

    # Find std.cppm
    if(CMAKE_CXX_COMPILER_VERSION MATCHES "^([0-9]+)")
        set(_clang_major "${CMAKE_MATCH_1}")
    endif()
    set(_std_module_paths
        "/usr/lib/llvm-${_clang_major}/share/libc++/v1/std.cppm"
        "/usr/local/share/libc++/v1/std.cppm"
    )
    foreach(_path ${_std_module_paths})
        if(EXISTS "${_path}")
            set(STD_MODULE_SOURCE "${_path}")
            get_filename_component(STD_MODULE_DIR "${_path}" DIRECTORY)
            message(STATUS "Found std module source: ${STD_MODULE_SOURCE}")
            break()
        endif()
    endforeach()
endif()

include(FetchContent)

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads QUIET)
if(NOT Threads_FOUND)
    message(WARNING "Threads not detected; creating fallback.")
    add_library(Threads::Threads INTERFACE IMPORTED)
    set_property(TARGET Threads::Threads PROPERTY INTERFACE_LINK_LIBRARIES "-lpthread")
    set(Threads_FOUND TRUE CACHE BOOL "" FORCE)
endif()

# Google Test
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# UnQLite
FetchContent_Declare(
    unqlite
    GIT_REPOSITORY https://github.com/symisc/unqlite.git
    GIT_TAG        master
)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(unqlite)
if(NOT unqlite_POPULATED)
    FetchContent_Populate(unqlite)
endif()

enable_testing()

# Test library with modules
add_library(test_modules STATIC)

# CXX_MODULE_STD: ON for Unix (Clang with libc++), OFF for MSVC (handles std module natively)
if(UNIX)
    set_target_properties(test_modules PROPERTIES CXX_MODULE_STD ON)
endif()

target_sources(test_modules
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${PARENT_DIR}
        FILES
            ${PARENT_DIR}/lib/types.cxx
            ${PARENT_DIR}/lib/storage.cxx
            ${PARENT_DIR}/modules/unqlite_module.ixx
            ${PARENT_DIR}/modules/indexer_module.ixx
    PRIVATE
        ${unqlite_SOURCE_DIR}/unqlite.c
)

# Add std module for Clang on Unix
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND UNIX AND DEFINED STD_MODULE_SOURCE)
    target_sources(test_modules
        PUBLIC
            FILE_SET cxx_modules_std
            TYPE CXX_MODULES
            BASE_DIRS "${STD_MODULE_DIR}"
            FILES "${STD_MODULE_SOURCE}"
    )
endif()

if(WIN32)
    target_sources(test_modules
        PUBLIC
            FILE_SET cxx_modules_win
            TYPE CXX_MODULES
            BASE_DIRS ${PARENT_DIR}
            FILES
                ${PARENT_DIR}/modules/process_module.ixx
    )
endif()

if(UNIX)
    target_sources(test_modules
        PUBLIC
            FILE_SET cxx_modules_unix
            TYPE CXX_MODULES
            BASE_DIRS ${PARENT_DIR}
            FILES
                ${PARENT_DIR}/lib/bfd_wrapper.cxx
    )
    target_link_libraries(test_modules PUBLIC bfd)
endif()

target_include_directories(test_modules PRIVATE 
    ${PARENT_DIR}
    ${unqlite_SOURCE_DIR}
)

target_compile_features(test_modules
    PRIVATE   cxx_std_23
    INTERFACE cxx_std_20
)

# Main test executable
add_executable(index_libs_test index_libs_test.cpp)

if(UNIX)
    set_target_properties(index_libs_test PROPERTIES CXX_MODULE_STD ON)
endif()

target_link_libraries(index_libs_test PRIVATE test_modules GTest::gtest_main Threads::Threads)
gtest_discover_tests(index_libs_test)

# BFD-specific tests (Linux only)
if(UNIX)
    add_executable(bfd_test bfd_test.cpp)
    set_target_properties(bfd_test PROPERTIES CXX_MODULE_STD ON)
    target_link_libraries(bfd_test PRIVATE test_modules bfd GTest::gtest_main Threads::Threads)
    gtest_discover_tests(bfd_test)
endif()