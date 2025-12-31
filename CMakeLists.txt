cmake_minimum_required(VERSION 3.28)

# Enable experimental import std support BEFORE project() for Clang/GCC
# The UUID depends on CMake version and must match exactly.
# Check Help/dev/experimental.rst in CMake source for the current value.
#
# On Windows/MSVC this is not needed - Visual Studio handles std module natively
# IMPORTANT: Both CMAKE_EXPERIMENTAL_CXX_IMPORT_STD and CMAKE_CXX_MODULE_STD
# must be set BEFORE project() call

if(UNIX)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    # CMake 3.30+ and 4.x: Try to detect the UUID automatically
    
    if(NOT DEFINED CMAKE_EXPERIMENTAL_CXX_IMPORT_STD)
      # Search in multiple potential locations for the experimental.rst file
      set(_search_paths
          "${CMAKE_ROOT}/Help/dev/experimental.rst"
          "${CMAKE_ROOT}/share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Help/dev/experimental.rst"
          "${CMAKE_ROOT}/../share/cmake-${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}/Help/dev/experimental.rst"
          # Common Linux/Snap paths
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
      # Fallback for 3.30 if we are on 3.30.x exactly
      if(CMAKE_VERSION VERSION_LESS "3.31")
         set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
      else()
         # For 4.x or newer dev versions, the UUID is likely different.
         # We cannot guess it safely.
         message(FATAL_ERROR "Could not determine CMAKE_EXPERIMENTAL_CXX_IMPORT_STD UUID for CMake ${CMAKE_VERSION}. "
                             "The 'import std' feature requires a UUID that changes with CMake versions. "
                             "Please set it manually with -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=<uuid> "
                             "or ensure Help/dev/experimental.rst is available in your CMake installation.")
      endif()
    endif()

    set(CMAKE_CXX_MODULE_STD 1)
  elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
    set(CMAKE_CXX_MODULE_STD 1)
  else()
    message(FATAL_ERROR "CMake 3.28+ is required for C++23 module support with import std")
  endif()
endif()

# Enable C language support so unqlite.c is compiled
project(index_libs VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Compiler-specific options (after project() when compiler is known)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(STATUS "MSVC detected: relying on Visual Studio/MSBuild to compile `std`/`std.compat` modules.")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT WIN32)
  # Ensure libc++ is used for ALL targets, including the implicit 'std' module target
  # This is critical: without this, the 'std' module target tries to build with libstdc++ and fails
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
  add_link_options($<$<LINK_LANGUAGE:CXX>:-stdlib=libc++>)

  # We use CMAKE_CXX_FLAGS in addition to add_compile_options because the latter does not always
  # propagate to the synthetic 'std' target in current CMake versions.
  # We also set linker flags explicitly to ensure consistency.
  if(NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++")
  endif()

  # Attempt to find clang-scan-deps if not automatically detected
  if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
    # Method 1: Ask the compiler driver (most reliable if compiler is configured correctly)
    execute_process(
      COMMAND "${CMAKE_CXX_COMPILER}" -print-prog-name=clang-scan-deps
      OUTPUT_VARIABLE _clang_scan_deps_prog_name
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(_clang_scan_deps_prog_name AND EXISTS "${_clang_scan_deps_prog_name}")
      set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${_clang_scan_deps_prog_name}" CACHE FILEPATH "clang-scan-deps executable" FORCE)
    endif()
  endif()

  if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
    # Method 2: Search in paths derived from compiler location and version
    get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_scan_deps_hints "${_compiler_dir}")
    
    set(_clang_major "")
    if(CMAKE_CXX_COMPILER_VERSION MATCHES "^([0-9]+)")
      set(_clang_major "${CMAKE_MATCH_1}")
      # Add standard LLVM installation paths for Linux (Ubuntu/Debian)
      list(APPEND _scan_deps_hints "/usr/lib/llvm-${_clang_major}/bin")
      
      find_program(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS 
                   NAMES clang-scan-deps-${_clang_major} clang-scan-deps
                   HINTS ${_scan_deps_hints}
                   DOC "clang-scan-deps executable")
    else()
      find_program(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS clang-scan-deps HINTS ${_scan_deps_hints})
    endif()
  endif()

  if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
      message(FATAL_ERROR "clang-scan-deps not found. It is required for C++ modules support with Clang.\n"
                          "Please install it (e.g. 'sudo apt install clang-tools-${_clang_major}' on Ubuntu) "
                          "or set CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS manually.")
  endif()

  # Find and add the std module source from libc++
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
    OUTPUT_VARIABLE _clang_resource_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  
  # Try common locations for std.cppm
  set(_std_module_paths
    "/usr/lib/llvm-${_clang_major}/share/libc++/v1/std.cppm"
    "/usr/local/share/libc++/v1/std.cppm"
    "${_clang_resource_dir}/../../../share/libc++/v1/std.cppm"
  )
  
  set(_std_module_found FALSE)
  foreach(_path ${_std_module_paths})
    if(EXISTS "${_path}")
      set(STD_MODULE_SOURCE "${_path}")
      set(_std_module_found TRUE)
      message(STATUS "Found std module source: ${STD_MODULE_SOURCE}")
      break()
    endif()
  endforeach()
  
  if(NOT _std_module_found)
    message(FATAL_ERROR "Could not find std.cppm for libc++ modules. "
                        "Ensure libc++ 18+ is installed with module support.")
  endif()
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
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

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

if(WIN32)
    target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET cxx_modules_win
            TYPE CXX_MODULES
            FILES
                modules/process_module.ixx
    )
endif()

if(UNIX)
    target_sources(${PROJECT_NAME}
        PUBLIC FILE_SET CXX_MODULES FILES
            lib/bfd_wrapper.cxx
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE bfd)
endif()

target_compile_features(${PROJECT_NAME}
  PRIVATE   cxx_std_23
  INTERFACE cxx_std_20
)

add_subdirectory(src)

# After the existing target_sources block, add the std module for Clang on Unix
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT WIN32 AND DEFINED STD_MODULE_SOURCE)
    # Get the directory containing std.cppm to use as BASE_DIRS
    get_filename_component(_std_module_dir "${STD_MODULE_SOURCE}" DIRECTORY)
    
    target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET cxx_modules_std
            TYPE CXX_MODULES
            BASE_DIRS "${_std_module_dir}"
            FILES "${STD_MODULE_SOURCE}"
    )
endif()
