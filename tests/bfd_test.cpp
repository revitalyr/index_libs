#include <bfd.h>

import std;
import bfd_test_module;

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cerr << "Lack input file!\n";
        return 1;
    }

    try {
        std::cout << "Input: " << argv[1] << '\n';
        auto file{BfdWrapper(argv[1])};

        if (file.is_format(bfd_archive)) {
            BfdWrapper arfile;

            std::cout << file.filename() << " contains following files:\n";

            while ((arfile = file.open_next_archive(arfile)) != false) {
                std::cout << arfile.filename() << '\n';

                arfile.scan_symbols([&arfile](bfd_symbol const *symbol) {
                    std::string_view name{symbol->name};
                    auto demangled = arfile.demangle(name);
                    std::cout << "  " << demangled << " (" << name << ")\n";
                });
            }
        } else {
            std::cerr << file.filename() << " is not archive!\n";
            return 1;
        }
    } catch (std::exception const &ex) {
        std::cerr << typeid(ex).name() << ": " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
