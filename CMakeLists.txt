cmake_minimum_required(VERSION 4.0 FATAL_ERROR)

# Enable C language support so unqlite.c is compiled
project(index_libs VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT WIN32)
  add_compile_options(-stdlib=libc++)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(STATUS "MSVC detected: relying on Visual Studio/MSBuild to compile `std`/`std.compat` modules.")
else()
  set(CMAKE_CXX_MODULE_STD 1)
endif()

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

include(ExternalProject)
include(FetchContent)

function(add_thirdparty name repo tag include)
    set(EPRG_NAME ${name})
    set(EPRG_REPO ${repo})
    set(EPRG_TAG  ${tag})

    set(EPRG_ROOT "${CMAKE_BINARY_DIR}/thirdparty/${EPRG_NAME}")
    set(EPRG_INCLUDE_DIR "${EPRG_ROOT}/Source/${EPRG_NAME}/${include}")
    set_directory_properties(PROPERTIES EP_BASE "${EPRG_ROOT}")

    ExternalProject_Add ( ${EPRG_NAME}
                          GIT_REPOSITORY ${EPRG_REPO}
                          GIT_TAG ${EPRG_TAG}
                          DOWNLOAD_EXTRACT_TIMESTAMP true
                          CONFIGURE_COMMAND ""
                          BUILD_COMMAND ""
                          INSTALL_COMMAND ""
                        )

    if (include)
      # Use target_include_directories to ensure the target gets the include path
      target_include_directories(${PROJECT_NAME} PUBLIC ${EPRG_INCLUDE_DIR})
    endif()
    add_dependencies(${PROJECT_NAME} ${EPRG_NAME})
endfunction()

add_executable(${PROJECT_NAME})

# Header-only dependencies
add_thirdparty(args       "https://github.com/Taywee/args"            "6.4.7" ".")
add_thirdparty(termcolor  "https://github.com/ikalnytskyi/termcolor"  "v2.1.0" "include")

# UnQLite via FetchContent (Source only, no add_subdirectory)
FetchContent_Declare(
  unqlite
  GIT_REPOSITORY https://github.com/symisc/unqlite.git
  GIT_TAG        master
)

# Allow FetchContent_Populate for manual source retrieval
cmake_policy(SET CMP0169 OLD)

FetchContent_GetProperties(unqlite)
if(NOT unqlite_POPULATED)
  FetchContent_Populate(unqlite)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${unqlite_SOURCE_DIR})

target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET CXX_MODULES FILES
        modules/args_module.ixx
        modules/unqlite_module.ixx
        modules/termcolor_module.ixx
        modules/indexer_module.ixx
        lib/types.cxx
        lib/storage.cxx
    PRIVATE
        # Add UnQLite source file from FetchContent location
        "${unqlite_SOURCE_DIR}/unqlite.c"
)

if (WIN32)
    target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET cxx_modules_win
            TYPE CXX_MODULES
            FILES
                modules/process_module.ixx
    )
endif()

target_compile_features(${PROJECT_NAME}
  PRIVATE   cxx_std_23
  INTERFACE cxx_std_20
)

add_subdirectory(lib)
add_subdirectory(src)
