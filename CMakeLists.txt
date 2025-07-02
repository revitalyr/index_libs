cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
set(CMAKE_CXX_FLAGS '-stdlib=libc++')

set(CMAKE_CXX_MODULE_STD 1)

project(index_libs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

#set(CMAKE_CXX_SCAN_FOR_MODULES ON)

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
    add_dependencies(${PROJECT_NAME} ${EPRG_NAME})
endfunction()

add_executable(${PROJECT_NAME})

add_thirdparty(args       "https://github.com/Taywee/args"            "6.4.7" "")
add_thirdparty(sqlite_orm "https://github.com/fnc12/sqlite_orm"       "v1.9.1" include)
add_thirdparty(termcolor  "https://github.com/ikalnytskyi/termcolor"  "v2.1.0" include)

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIR})

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
list(JOIN dirs : includes)
message(STATUS "INCLUDE_DIRECTORIES='${dirs}'")
message(STATUS "includes='${includes}'")
set(ENV{CPLUS_INCLUDE_PATH} ${includes})

target_sources(${PROJECT_NAME}
    PRIVATE FILE_SET CXX_MODULES FILES
        modules/args_module.ixx
        modules/sqlite_orm_module.ixx
        modules/termcolor_module.ixx
)

if (WIN32)
    target_sources(${PROJECT_NAME}
        PRIVATE FILE_SET CXX_MODULES FILES
            modules/boost_module.ixx
    )
endif (WIN32)

target_compile_features(${PROJECT_NAME}
  PRIVATE   cxx_std_23
  INTERFACE cxx_std_20)

add_subdirectory(src)