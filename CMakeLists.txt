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

function(add_thirdparty name repo tag include)
    set(EPRG_NAME ${name})
    set(EPRG_REPO ${repo})
    set(EPRG_TAG  ${tag})

    set(EPRG_ROOT "${CMAKE_BINARY_DIR}/thirdparty/${EPRG_NAME}")
    set(EPRG_INCLUDE_DIR "${EPRG_ROOT}/Source/${EPRG_NAME}//${include}")
    set_directory_properties(PROPERTIES EP_BASE "${EPRG_ROOT}")

    ExternalProject_Add ( ${EPRG_NAME}
                          GIT_REPOSITORY ${EPRG_REPO}
                          GIT_TAG ${EPRG_TAG}
                          DOWNLOAD_EXTRACT_TIMESTAMP true
                          CONFIGURE_COMMAND ""
                          BUILD_COMMAND ""
                          INSTALL_COMMAND ""
                        )

    include_directories(${EPRG_INCLUDE_DIR})
endfunction()

add_thirdparty(args       "https://github.com/Taywee/args"            "6.4.7" "")
add_thirdparty(sqlite_orm "https://github.com/fnc12/sqlite_orm"       "v1.9.1" include)
add_thirdparty(termcolor  "https://github.com/ikalnytskyi/termcolor"  "v2.1.0" include)

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIR})

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
    PRIVATE FILE_SET CXX_MODULES FILES
        modules/args_module.ixx
        modules/sqlite_orm_module.ixx
        modules/termcolor_module.ixx
)

if (WIN32)
    target_sources(index_libs
        PRIVATE FILE_SET CXX_MODULES FILES
            modules/boost_module.ixx
    )
endif (WIN32)

target_compile_features(index_libs 
  PRIVATE   cxx_std_23
  INTERFACE cxx_std_20)

