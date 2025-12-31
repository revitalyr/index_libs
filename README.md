# index_libs

Build an index of symbols exported from static libraries (COFF/ELF archives) and search for libraries by symbol name.

## Features

- Index symbols from `.lib` files (Windows) or `.a` files (Linux)
- Search for symbols by substring (case-sensitive)
- Demangle C++ symbol names for readable output
- Export database to CSV format
- Uses UnQLite embedded database for persistent storage
- Cross-platform: Windows and Linux

## Requirements

- CMake 3.28 or higher
- C++23 compatible compiler with modules support:
  - **Windows**: Visual Studio 2022 (17.6+)
  - **Linux**: Clang 18+ with libc++

### Linux Prerequisites

Install required build tools before running CMake:

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install ninja-build cmake clang libc++-dev libc++abi-dev binutils-dev

# Fedora/RHEL
sudo dnf install ninja-build cmake clang libcxx-devel binutils-devel

# Arch Linux
sudo pacman -S ninja cmake clang libc++
```

### Windows Prerequisites

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload
2. (Optional) Install [vcpkg](https://github.com/microsoft/vcpkg) for dependency management

## Building

### Windows with Visual Studio and vcpkg

Configure the project using CMake with vcpkg toolchain:

```cmd
cmake -S . -B build -G "Visual Studio 17 2022" ^
    -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DCMAKE_PREFIX_PATH=<path-to-vcpkg>/installed/x64-windows
```

Build the project:

```cmd
cmake --build build --config Release
```

### Windows without vcpkg

If you don't use vcpkg, dependencies are fetched automatically via CMake FetchContent:

```cmd
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Linux

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Usage

```
index_libs {OPTIONS} [symbol]

OPTIONS:
    -h, --help                Display this help menu 
    -b, --build <path>        Path to the library (or directory of libraries) 
                              for which the index will be built
    -d, --database <path>     Path to the database file
    -e, --export <file>       Export database to CSV file (use - for stdout)
    -E                        Export database to stdout as CSV
    -v, --verbose             Print symbols while indexing
    symbol                    Symbol substring to find (case-sensitive)
```

### Examples

Build index for a single library:

```
index_libs -b path/to/library.lib
```

Build index for all libraries in a directory:

```
index_libs -b path/to/libs/
```

Search for a symbol:

```
index_libs printf
index_libs "std::vector"
index_libs "operator new"
```

Build index and search in one command:

```
index_libs -b path/to/libs/ malloc
```

Export database to CSV:

```
index_libs -e symbols.csv
index_libs -E > symbols.csv
```

## Symbol Search Format

The symbol argument is a substring to search for. The search matches any symbol
or unmangled name that contains the given substring (case-sensitive).

Examples:
- `printf` - finds all symbols containing "printf"
- `std::vector` - finds all symbols containing "std::vector"
- `operator new` - finds all symbols containing "operator new"

## Notes

- On Windows, the tool parses COFF archives directly without external dependencies
- On Linux, the tool uses BFD library for parsing `.a` archives
- The database is stored by default in the system temp directory
