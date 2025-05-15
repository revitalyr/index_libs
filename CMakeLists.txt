cmake_minimum_required(VERSION 3.28)
project(index_libs VERSION 0.1.0 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

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
