# index_libs

Building an index of symbols exported from COFF libraries and searching for libraries by symbol.
This uses _dumpbin.exe_, which should be available in %_PATH_%.

_index_libs.exe_ -h gives:

  index_libs.exe {OPTIONS} [SYMBOL]
  
    Building an index of symbols exported from COFF libraries and searching for libraries by symbol.

  OPTIONS:

      -h, --help                        Display this help menu
      This arguments are exclusive:
        -b[BUILD], --build=[BUILD]        Path to the library for which the index is built
        SYMBOL                            Symbol to find
      "--" can be used to terminate flag options and force all following arguments to be treated as positional options

## Features

- Index symbols from `.lib` files (Windows) or `.a` files (Linux)
- Search for symbols by substring (case-sensitive)
- Export database to CSV format
- Uses UnQLite embedded database for storage

## Requirements

- CMake 4.0 or higher
- C++23 compatible compiler (Visual Studio 2022, Clang 18+, GCC 14+)
- vcpkg (optional, for dependency management)

## Building

### Windows with Visual Studio and vcpkg

Configure the project using CMake with vcpkg toolchain:
`cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_PREFIX_PATH=<path-to-vcpkg>/installed/x64-windows`

Build the project:
`cmake --build build --config Release`

### Windows without vcpkg

If you don't use vcpkg, dependencies are fetched automatically via CMake FetchContent:
`cmake -S . -B build`

### Linux
`cmake -S . -B build -G Ninja cmake --build build`

## Usage

`index_libs.exe {OPTIONS} [symbol]
    OPTIONS: -h, --help                Display this help menu 
             -b, --build <path>        Path to the library (or directory of libraries) for which the index will be built
             -d, --database <path>     Path to the database file
             -e, --export <file>       Export database to CSV file (use - for stdout)
             -E                        Export database to stdout as CSV
             -v, --verbose             Print symbols while indexing
             symbol                    Symbol substring to find (case-sensitive)`

### Examples

Build index for a single library:
`index_libs -b path/to/library.lib`

Build index for all libraries in a directory:
`index_libs -b path/to/libs/`

Search for a symbol:
`index_libs printf index_libs "std::vector" index_libs "operator new"`

Build index and search in one command:
`index_libs -b path/to/libs/ malloc`

## Export database to CSV
index_libs -e symbols.csv index_libs -E > symbols.csv

## Symbol Search Format

The symbol argument is a substring to search for. The search matches any symbol
or unmangled name that contains the given substring (case-sensitive).

Examples:
- `printf` - finds all symbols containing "printf"
- `std::vector` - finds all symbols containing "std::vector"
- `operator new` - finds all symbols containing "operator new"

## Notes

- On Windows, the tool parses COFF archives directly without external dependencies
- The database is stored by default in the system temp directory
