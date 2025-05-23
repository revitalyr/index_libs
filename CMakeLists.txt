cmake_minimum_required(VERSION 4.0)
project(index_libs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(ExternalProject)

function(add_thirdparty name url include)
    set(PACK_ROOT ${CMAKE_BINARY_DIR}/thirdparty/${name})
    set(PACK_INCLUDE_DIR ${PACK_ROOT}/${include})

    ExternalProject_Add ( ${name}
                        PREFIX ${PACK_ROOT}
                        STAMP_DIR ${PACK_ROOT}
                        SOURCE_DIR ${PACK_ROOT}
                        DOWNLOAD_DIR ${PACK_ROOT}
                        URL ${url}
                        DOWNLOAD_EXTRACT_TIMESTAMP true
                        CONFIGURE_COMMAND ""
                        BUILD_COMMAND ""
                        )
endfunction()

add_thirdparty(args "https://github.com/Taywee/args/archive/refs/tags/6.4.7.tar.gz" "")
add_thirdparty(sqlite_orm "https://github.com/fnc12/sqlite_orm/archive/refs/tags/v1.9.1.tar.gz" include)
add_thirdparty(termcolor "https://github.com/ikalnytskyi/termcolor/archive/refs/tags/v2.1.0.tar.gz" include)

add_executable(index_libs index_libs.cpp storage.cpp)

target_include_directories(index_libs PRIVATE ${ARGS_INCLUDE_DIR})
target_include_directories(index_libs PRIVATE ${SQLITE_ORM_INCLUDE_DIR})
target_include_directories(index_libs PRIVATE ${TERMCOLOR_INCLUDE_DIR})

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
