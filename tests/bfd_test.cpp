#include <bfd.h>
#include <gtest/gtest.h>
#include <stdio.h>

import std;
import types;
import bfd_wrapper_module;

auto out = &std::cerr;

Strings exec(StrView cmd) {
    std::array<char, 1024> buffer;
    std::string output;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd.data(), "r"), pclose
    );
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
           nullptr) {
        output += buffer.data();
    }

    Strings result;
    auto sstream{std::stringstream{output}};

    for (std::string line; std::getline(sstream, line, '\n');) {
        std::cout << line << '\n';
    }

    return result;
}

auto t() {
    auto lib{
        BfdWrapper{"/mnt/d/work/codebrowser_fork/index_libs/tests/data/"
                   "libLLVMDWARFLinker.a"}
    };

    String cmd{"nm -C "};
    auto checked{exec(cmd + lib.filename())};
    std::ranges::for_each(checked, [](auto const &line) {
        std::cout << line << '\n';
    });

    *out << lib.filename() << ":\n";
    for (auto const &bfd : BfdRange(lib)) {
        *out << "  " << bfd.filename() << '\n';
        for (auto const symbol : bfd.symbols()) {
            std::string_view name{symbol->name};
            auto demangled = bfd.demangle(name);
            std::cout << "    " << demangled << " (" << name << ")\n";
        };
    }
}

namespace TestData {
    std::filesystem::path THIS_FILE{__FILE__};
    String const LIB{
        (THIS_FILE.parent_path() / "data/libLLVMDWARFLinker.a").string()
    };
};  // namespace TestData

struct BfdWrapperTestsF : public testing::Test {
    BfdWrapper m_file;
    BfdWrapperTestsF() : m_file{TestData::LIB} { t(); }
};

TEST_F(BfdWrapperTestsF, NotExistenFile) {
    EXPECT_THROW(BfdWrapper("not a file"), Exception::CouldNotOpen);
}

TEST_F(BfdWrapperTestsF, IsArchive) {
    ASSERT_TRUE(m_file.is_format(bfd_archive));
}

/*
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
*/
