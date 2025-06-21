cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

# Set experimental flag to enable `import std` support from CMake.
# This must be enabled before C++ language support.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
  # This specific value changes as experimental support evolves. See
  # `Help/dev/experimental.rst` in the CMake source corresponding to
  # your CMake build for the exact value to use.
  "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")

set(CMAKE_CXX_STANDARD 23)
set(CXX_MODULE_STD True)

project(index_libs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(ExternalProject)

function(add_thirdparty name url include)
    set(PACK_ROOT ${CMAKE_BINARY_DIR}/thirdparty/${name})

    ExternalProject_Add ( ${name}
                        PREFIX ${PACK_ROOT}
                        STAMP_DIR ${CMAKE_BINARY_DIR}/thirdparty
                        SOURCE_DIR ${PACK_ROOT}
                        DOWNLOAD_DIR ${PACK_ROOT}
                        URL ${url}
                        DOWNLOAD_EXTRACT_TIMESTAMP true
                        CONFIGURE_COMMAND ""
                        BUILD_COMMAND ""
                        )

    include_directories(${PACK_ROOT}/${include})
endfunction()

add_thirdparty(args "https://github.com/Taywee/args/archive/refs/tags/6.4.7.tar.gz" "")
add_thirdparty(sqlite_orm "https://github.com/fnc12/sqlite_orm/archive/refs/tags/v1.9.1.tar.gz" include)
add_thirdparty(termcolor "https://github.com/ikalnytskyi/termcolor/archive/refs/tags/v2.1.0.tar.gz" include)

find_package(SQLite3 REQUIRED)


#target_include_directories(index_libs PRIVATE ${ARGS_INCLUDE_DIR})
#target_include_directories(index_libs PRIVATE ${SQLITE_ORM_INCLUDE_DIR})
#target_include_directories(index_libs PRIVATE ${TERMCOLOR_INCLUDE_DIR})

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
list(JOIN dirs : includes)
message(STATUS "INCLUDE_DIRECTORIES='${dirs}'")
message(STATUS "includes='${includes}'")
set(ENV{CPLUS_INCLUDE_PATH} ${includes})

add_executable(index_libs index_libs.cpp storage.cpp)

target_sources(index_libs
    PUBLIC
        FILE_SET CXX_MODULES
        FILES
            modules/args_module.ixx
            modules/boost_module.ixx
            modules/sqlite_orm_module.ixx
            modules/termcolor_module.ixx
)

target_compile_features(index_libs 
  PRIVATE   cxx_std_23
  INTERFACE cxx_std_20)

