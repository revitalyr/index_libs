// work.cpp

import std;
#include <cassert>
#include <cstdlib>

namespace fs = std::filesystem;

using LibPath = fs::path;

struct Indexer {
    LibPath     m_libpath;
};

int main(int argc, char* argv[]) {
    try {
    }
    catch (std::exception const & e) {
        std::cerr << typeid(e).name() << ": " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}