module;

#include <bfd.h>

#include <cassert>

export module bfd_wrapper_module;

import std;
import types;

#ifndef DMGL_PARAMS
#define DMGL_NO_OPTS 0       /* For readability... */
#define DMGL_PARAMS (1 << 0) /* Include function args */
#define DMGL_ANSI (1 << 1)   /* Include const, volatile, etc */
#endif

// export bfd_format;

export namespace Exception {

    struct CouldNotOpen : public std::runtime_error {
        explicit CouldNotOpen(StrView filename)
            : std::runtime_error(std::format("Could not open '{}': {}", filename, bfd_errmsg(bfd_get_error()))) {}
    };

    struct BfdError : public std::runtime_error {
        explicit BfdError(StrView reason)
            : std::runtime_error(std::format("{}: {}", reason, bfd_errmsg(bfd_get_error()))) {}
    };

}  // namespace Exception

export struct BfdWrapper {
    bfd *m_bfd{nullptr};

    BfdWrapper() = default;

    explicit BfdWrapper(StrView filename) {
        static bool init = []() {
            bfd_init();
            return true;
        }();

        if (!(m_bfd = bfd_openr(filename.data(), nullptr))) {
            throw Exception::CouldNotOpen(filename);
        }
    }

    explicit BfdWrapper(bfd *bfd) : m_bfd{bfd} {
        if (!(bfd_check_format(m_bfd, bfd_object) ||
              bfd_check_format(m_bfd, bfd_archive))) {
            throw Exception::BfdError("BfdWrapper: Bad source bfd");
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

    BfdWrapper open_next_archive(BfdWrapper const &previous) {
        assert(is_format(bfd_archive));
        if (auto next_arc{bfd_openr_next_archived_file(m_bfd, previous.m_bfd)};
            next_arc != nullptr) {
            return BfdWrapper{next_arc};
        }

        if (bfd_get_error() != bfd_error_no_more_archived_files) {
            throw Exception::BfdError("open_next_archive");
        }

        return BfdWrapper{};
    }

    void close() noexcept {
        if (*this) {
            bfd_close(m_bfd);
            m_bfd = nullptr;
        }
    }

    using SymbolTable = std::vector<bfd_symbol *>;

    SymbolTable symbols() const {
        SymbolTable symbol_table;

        if (auto storage_needed = bfd_get_symtab_upper_bound(m_bfd);
            storage_needed > 0) {
            symbol_table.resize(
                storage_needed / sizeof(SymbolTable::value_type)
            );

            if (auto number_of_symbols = bfd_canonicalize_symtab(m_bfd, symbol_table.data()); number_of_symbols > 0) {
                symbol_table.resize(number_of_symbols);
            } else {
                throw Exception::BfdError("symbols");
            }
        }

        return symbol_table;
    }

    StrView demangle(StrView name) const noexcept {
        if (name.starts_with('.')) {
            if (auto pos = name.find_last_of('.'); pos != name.npos) {
                name.remove_prefix(pos + 1);
            }
        }
        auto verbose{true};
        int demangle_flags = verbose ? (DMGL_PARAMS | DMGL_ANSI) : DMGL_NO_OPTS;
        auto demangled = bfd_demangle(m_bfd, name.data(), demangle_flags);
        return (demangled ? demangled : "");
    }

    bool is_format(bfd_format format) const noexcept {
        return bfd_check_format(m_bfd, format);
    }

    char const *filename() const noexcept {
        return m_bfd ? m_bfd->filename : "";
    }

    bfd const *get() const noexcept { return m_bfd; }
};

export struct BfdRange {
    BfdWrapper &m_bfd;

    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = BfdWrapper &;
        using difference_type = std::ptrdiff_t;
        struct Sentinel {};

        BfdWrapper &m_bfd;
        BfdWrapper m_archive;

        Iterator(BfdWrapper &bfd) : m_bfd{bfd} {
            operator++();
        }

        bool operator==(Sentinel) { return !m_archive; }
        Iterator &operator++() {
            if ((m_archive = m_bfd.open_next_archive(m_archive)) == false && bfd_get_error() != bfd_error_no_more_archived_files) {
                bfd_perror("failed");
            }
            return *this;
        }
        value_type operator*() { return m_archive; }
    };

    BfdRange(BfdWrapper &bfd) : m_bfd{bfd} {
    }

    Iterator begin() { return Iterator{m_bfd}; }
    Iterator::Sentinel end() const { return Iterator::Sentinel{}; }
};
