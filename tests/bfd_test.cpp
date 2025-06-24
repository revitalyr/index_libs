#include <bfd.h>

#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

#ifndef DMGL_PARAMS
#define DMGL_NO_OPTS 0       /* For readability... */
#define DMGL_PARAMS (1 << 0) /* Include function args */
#define DMGL_ANSI (1 << 1)   /* Include const, volatile, etc */
#endif

struct BfdWrapper {
    bfd *m_bfd{nullptr};

    BfdWrapper() = default;

    explicit BfdWrapper(std::string_view filename) {
        static bool init = []() {
            bfd_init();
            return true;
        }();

        if (!(m_bfd = bfd_openr(filename.data(), nullptr))) {
            throw std::runtime_error((std::stringstream()
                                      << "Could not open '" << filename
                                      << "': " << bfd_errmsg(bfd_get_error()))
                                         .str());
        }
    }

    explicit BfdWrapper(bfd *bfd) : m_bfd{bfd} {
        if (!(bfd_check_format(m_bfd, bfd_object) ||
              bfd_check_format(m_bfd, bfd_archive))) {
            throw std::runtime_error("Bad source bfd");
        }
    }

    ~BfdWrapper() noexcept { close(); }

    BfdWrapper &operator=(BfdWrapper &&rlh) noexcept {
        close();
        m_bfd = rlh.m_bfd;
        rlh.m_bfd = nullptr;
        return *this;
    }

    operator bool() const noexcept { return m_bfd != nullptr; }

    BfdWrapper open_next_archive(BfdWrapper const &previous) noexcept {
        if (auto next_arc{bfd_openr_next_archived_file(m_bfd, previous.m_bfd)};
            next_arc != nullptr) {
            return BfdWrapper{next_arc};
        }
        return BfdWrapper{};
    }

    void close() noexcept {
        if (*this) {
            bfd_close(m_bfd);
            m_bfd = nullptr;
        }
    }

    bool is_format(bfd_format format) const noexcept {
        return bfd_check_format(m_bfd, format);
    }

    char const *filename() const noexcept {
        return m_bfd ? m_bfd->filename : "";
    }

    bfd const *get() const noexcept { return m_bfd; }
};

using SymbolTable = std::vector<bfd_symbol *>;

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cerr << "Lack input file!\n";
        return 1;
    }

    try {
        std::cout << "Input: " << argv[1] << '\n';
        auto file{BfdWrapper(argv[1])};

        if (file.is_format(bfd_archive)) {
            auto verbose{true};
            int demangle_flags =
                verbose ? (DMGL_PARAMS | DMGL_ANSI) : DMGL_NO_OPTS;
            BfdWrapper arfile;

            std::cout << file.filename() << " contains following files:\n";

            while ((arfile = file.open_next_archive(arfile)) != false) {
                std::cout << arfile.filename() << '\n';

                auto abfd{const_cast<bfd *>(arfile.get())};

                if (auto storage_needed = bfd_get_symtab_upper_bound(abfd);
                    storage_needed > 0) {
                    SymbolTable symbol_table(
                        storage_needed / sizeof(SymbolTable::value_type)
                    );

                    if (auto number_of_symbols =
                            bfd_canonicalize_symtab(abfd, symbol_table.data());
                        number_of_symbols > 0) {

                        symbol_table.resize(number_of_symbols);
                        for (auto symbol : symbol_table) {
                            std::string_view name{symbol->name};
                            if (name.starts_with('.')) {
                                if (auto pos = name.find_last_of('.');
                                    pos != name.npos) {
                                    name.remove_prefix(pos + 1);
                                }
                            }
                            auto demangled =
                                bfd_demangle(abfd, name.data(), demangle_flags);
                            std::cout << "  " << (demangled ? demangled : "")
                                      << " (" << name << ")\n";
                        }
                    }
                }
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
