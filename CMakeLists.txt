cmake_minimum_required(VERSION 4.0)
project(index_libs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(ExternalProject)

set(ARGS_ROOT ${CMAKE_BINARY_DIR}/thirdparty/args)
set(ARGS_INCLUDE_DIR ${ARGS_ROOT})

ExternalProject_Add ( args
                      PREFIX ${ARGS_ROOT}
                      STAMP_DIR ${ARGS_ROOT}
                      SOURCE_DIR ${ARGS_ROOT}
                      DOWNLOAD_DIR ${ARGS_ROOT}
                      URL "https://github.com/Taywee/args/archive/refs/tags/6.4.7.tar.gz"
                      DOWNLOAD_EXTRACT_TIMESTAMP true
                      CONFIGURE_COMMAND ""
                      BUILD_COMMAND ""
                    )

set(SQLITE_ORM_ROOT ${CMAKE_BINARY_DIR}/thirdparty/sqlite_orm)
set(SQLITE_ORM_INCLUDE_DIR ${SQLITE_ORM_ROOT}/include)

ExternalProject_Add ( sqlite_orm
                      PREFIX ${SQLITE_ORM_ROOT}
                      STAMP_DIR ${SQLITE_ORM_ROOT}
                      SOURCE_DIR ${SQLITE_ORM_ROOT}
                      DOWNLOAD_DIR ${SQLITE_ORM_ROOT}
                      URL "https://github.com/fnc12/sqlite_orm/archive/refs/tags/v1.9.1.tar.gz"
                      DOWNLOAD_EXTRACT_TIMESTAMP true
                      CONFIGURE_COMMAND ""
                      BUILD_COMMAND ""
                    )                    

add_executable(index_libs index_libs.cpp storage.cpp)

target_include_directories(index_libs PRIVATE ${ARGS_INCLUDE_DIR})
target_include_directories(index_libs PRIVATE ${SQLITE_ORM_INCLUDE_DIR})

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
