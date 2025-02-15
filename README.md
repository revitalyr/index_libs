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
