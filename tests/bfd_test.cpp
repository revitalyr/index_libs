#include <bfd.h>
#include <gtest/gtest.h>

import std;
import types;
import bfd_test_module;

auto out = &std::cerr;

struct BfdRange {
    BfdWrapper &m_bfd;

    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = BfdWrapper &;
        using difference_type = std::ptrdiff_t;
        struct Sentinel {};

        BfdWrapper &m_bfd;
        BfdWrapper m_archive;

        Iterator(BfdWrapper &bfd) : m_bfd{bfd} {
            //*out << "Iterator: " << m_bfd.filename() << '\n';
            operator++();
        }

        bool operator==(Sentinel) { return !m_archive; }
        Iterator &operator++() {
            m_archive = m_bfd.open_next_archive(m_archive);
            // *out << "operator++ ";
            // if (m_archive) {
            //     *out << m_archive.filename();
            // } else {
            //     bfd_perror("failed");
            // }
            // *out << '\n';
            return *this;
        }
        value_type operator*() { return m_archive; }
    };

    BfdRange(BfdWrapper &bfd) : m_bfd{bfd} {
        //        *out << "BfdRange: " << m_bfd.filename() << "\n";
    }

    Iterator begin() { return Iterator{m_bfd}; }
    Iterator::Sentinel end() const { return Iterator::Sentinel{}; }
};

auto t() {
    auto lib{
        BfdWrapper{"/mnt/d/work/codebrowser_fork/index_libs/tests/data/"
                   "libLLVMDWARFLinker.a"}
    };

    *out << lib.filename() << ":\n";
    for (auto const &bfd : BfdRange(lib)) {
        *out << "  " << bfd.filename() << '\n';
        bfd.scan_symbols([&bfd](bfd_symbol const *symbol) {
            std::string_view name{symbol->name};
            auto demangled = bfd.demangle(name);
            std::cout << "    " << demangled << " (" << name << ")\n";
        });
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
