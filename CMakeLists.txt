cmake_minimum_required(VERSION 3.28)
project(index_libs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(ExternalProject)

cmake_path(GET CMAKE_BINARY_DIR PARENT_PATH MASTER_CMAKE_DIR)
set(ARGS_ROOT ${MASTER_CMAKE_DIR}/thirdparty/args)
set(ARGS_INCLUDE_DIR ${ARGS_ROOT})

ExternalProject_Add ( args
                      PREFIX ${ARGS_ROOT}
#                      STAMP_DIR ${ARGS_ROOT}
#                      SOURCE_DIR ${ARGS_ROOT}
#                      DOWNLOAD_DIR ${ARGS_ROOT}
                      URL "https://github.com/Taywee/args/archive/refs/tags/6.4.7.tar.gz"
                      DOWNLOAD_EXTRACT_TIMESTAMP true
                      CONFIGURE_COMMAND ""
                      BUILD_COMMAND ""
                    )

include_directories(${ARGS_INCLUDE_DIR})

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

target_compile_features(index_libs PUBLIC cxx_std_23)
