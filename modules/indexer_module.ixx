module;

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

#if !__linux__
#    include <windows.h>
#    include <dbghelp.h>
#    pragma comment(lib, "Dbghelp.lib")
#endif

export module indexer_module;

import std;
import types;
import storage_module;

#if __linux__
import bfd_wrapper_module;
#endif

namespace fs = std::filesystem;

export namespace indexer {

#if __linux__
    inline constexpr StrView lib_extension{ ".a" };
#else
    inline constexpr StrView lib_extension{ ".lib" };
#endif

    // Demangle a mangled symbol name
    std::string demangle(std::string const& mangled, bool verbose = false);

    // Build index for a single library file
    void build_index(fs::path const& path, db::Storage& storage, bool verbose);

}  // namespace indexer

// Implementation details in anonymous namespace
namespace {

#if __linux__
    StrView extract_symbol(StrView s) {
        auto pos_bracket{ s.find('(') };
        auto beginning{ s.substr(0, pos_bracket) };
        auto pos_colon{ beginning.rfind(':') };
        return beginning.substr(pos_colon + 1);
    }

#else
    constexpr std::string_view ARCHIVE_MAGIC{ "!<arch>\n" };
    constexpr std::string_view ARCHIVE_THIN_MAGIC{ "!<thin>\n" };

    std::string read_file(fs::path const& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            throw std::runtime_error(std::format("Cannot open '{}'", path.string()));
        }
        std::string data(std::istreambuf_iterator<char>(in), {});
        return data;
    }

    std::size_t short_len(char const* p, std::size_t max) {
        std::size_t n = 0;
        while ((n < max) && p[n]) {
            ++n;
        }
        return n;
    }

    std::string try_demangle_itanium(std::string const& mangled, bool verbose) {
        if (mangled.size() < 3 || mangled[0] != '_' || mangled[1] != 'Z') {
            if (verbose) std::cout << "     [itanium] rejected: not _Z prefix\n";
            return {};
        }

        if (verbose) std::cout << "     [itanium] parsing: " << mangled << "\n";

        std::size_t pos = 2;
        std::vector<std::string> substitutions;

        auto parse_len = [&](std::size_t& p) -> std::size_t {
            std::size_t val = 0;
            if (p >= mangled.size() || !std::isdigit(static_cast<unsigned char>(mangled[p]))) {
                return 0;
            }
            while (p < mangled.size() && std::isdigit(static_cast<unsigned char>(mangled[p]))) {
                val = val * 10 + static_cast<std::size_t>(mangled[p] - '0');
                ++p;
            }
            return val;
        };

        auto parse_nested = [&](std::size_t& p) -> std::vector<std::string> {
            std::vector<std::string> parts;
            if (p >= mangled.size() || mangled[p] != 'N') {
                if (verbose) std::cout << "     [itanium] parse_nested: not 'N' at pos=" << p << "\n";
                return parts;
            }
            ++p;
            if (verbose) std::cout << "     [itanium] parse_nested: started at pos=" << p << "\n";
            
            while (p < mangled.size() && mangled[p] != 'E') {
                if (verbose) std::cout << "     [itanium] parse_nested: loop at pos=" << p << " char='" << mangled[p] << "'\n";
                
                if (mangled[p] == 'S' && (p + 1) < mangled.size() && mangled[p + 1] == '_') {
                    if (!substitutions.empty()) {
                        parts.push_back(substitutions.front());
                        if (verbose) std::cout << "     [itanium] parse_nested: substitution S_ -> " << substitutions.front() << "\n";
                    } else {
                        if (verbose) std::cout << "     [itanium] parse_nested: substitution S_ but no substitutions available\n";
                    }
                    p += 2;
                    continue;
                }
                
                std::size_t len = parse_len(p);
                if (verbose) std::cout << "     [itanium] parse_nested: parsed len=" << len << " at pos=" << p << "\n";
                
                if (len == 0 || (p + len) > mangled.size()) {
                    if (verbose) std::cout << "     [itanium] parse_nested: breaking - len=" << len << " remaining=" << (mangled.size() - p) << "\n";
                    break;
                }
                
                std::string component = mangled.substr(p, len);
                parts.emplace_back(component);
                substitutions.push_back(component);
                if (verbose) std::cout << "     [itanium] parse_nested: component[" << (parts.size() - 1) << "]=" << component << "\n";
                p += len;
            }
            
            if (p < mangled.size() && mangled[p] == 'E') {
                ++p;
                if (verbose) std::cout << "     [itanium] parse_nested: found 'E' terminator, parts=" << parts.size() << "\n";
            } else {
                if (verbose) std::cout << "     [itanium] parse_nested: no 'E' found at pos=" << p << ", parts=" << parts.size() << "\n";
            }
            return parts;
        };

        std::vector<std::string> name_parts;
        if (pos < mangled.size() && mangled[pos] == 'N') {
            if (verbose) std::cout << "     [itanium] detected nested name at pos=" << pos << "\n";
            name_parts = parse_nested(pos);
        } else if (pos < mangled.size() && std::isdigit(static_cast<unsigned char>(mangled[pos]))) {
            if (verbose) std::cout << "     [itanium] detected simple name at pos=" << pos << "\n";
            std::size_t len = parse_len(pos);
            if (len > 0 && (pos + len) <= mangled.size()) {
                name_parts.push_back(mangled.substr(pos, len));
                substitutions.push_back(name_parts.back());
                pos += len;
                if (verbose) std::cout << "     [itanium] simple name: " << name_parts.back() << "\n";
            }
        } else {
            if (verbose) std::cout << "     [itanium] unknown format at pos=" << pos << " char='" << (pos < mangled.size() ? mangled[pos] : '?') << "'\n";
        }

        if (name_parts.empty()) {
            if (verbose) std::cout << "     [itanium] FAILED: no name parts extracted\n";
            return {};
        }

        std::string out;
        for (std::size_t i = 0; i < name_parts.size(); ++i) {
            if (i) out += "::";
            out += name_parts[i];
        }

        if (verbose) std::cout << "     [itanium] SUCCESS: " << out << "\n";
        return out;
    }

    std::string try_demangle_itanium_names_only(std::string const& mangled) {
        if (mangled.size() < 4 || mangled[0] != '_' || mangled[1] != 'Z') {
            return {};
        }
        
        std::size_t p = 2;
        
        if (p >= mangled.size() || mangled[p] != 'N') {
            if (p < mangled.size() && std::isdigit(static_cast<unsigned char>(mangled[p]))) {
                std::size_t len = 0;
                while (p < mangled.size() && std::isdigit(static_cast<unsigned char>(mangled[p]))) {
                    len = len * 10 + static_cast<std::size_t>(mangled[p] - '0');
                    ++p;
                }
                if (len > 0 && (p + len) <= mangled.size()) {
                    return mangled.substr(p, len);
                }
            }
            return {};
        }
        
        ++p;
        
        std::vector<std::string> parts;
        while (p < mangled.size() && mangled[p] != 'E') {
            while (p < mangled.size() && !std::isdigit(static_cast<unsigned char>(mangled[p])) && mangled[p] != 'E') {
                ++p;
            }
            
            if (p >= mangled.size() || mangled[p] == 'E') {
                break;
            }
            
            std::size_t len = 0;
            while (p < mangled.size() && std::isdigit(static_cast<unsigned char>(mangled[p]))) {
                len = len * 10 + static_cast<std::size_t>(mangled[p] - '0');
                ++p;
            }
            
            if (len == 0) {
                return {};
            }
            
            if (p + len > mangled.size()) {
                return {};
            }
            
            parts.emplace_back(mangled.substr(p, len));
            p += len;
        }
        
        if (parts.empty()) {
            return {};
        }
        
        std::string out;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i) out += "::";
            out += parts[i];
        }
        return out;
    }

    std::string demangle_verbose(std::string const& mangled, bool verbose) {
        auto pretty = indexer::demangle(mangled, verbose);
        if (verbose) {
            std::cout << "   demangle: " << mangled << " -> " << pretty;
            std::cout << " [len=" << mangled.size() << "]";
            
            if (pretty == mangled) {
                std::cout << " (UNCHANGED)";
            }
            std::cout << '\n';
        }
        return pretty;
    }

    std::uint32_t read_be_u32(char const* p) {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(p[0])) << 24) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(p[1])) << 16) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(p[2])) << 8) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(p[3])));
    }

    std::string resolve_member_name(IMAGE_ARCHIVE_MEMBER_HEADER const& hdr, std::string_view longnames, bool verbose) {
        std::string name(reinterpret_cast<char const*>(hdr.Name), sizeof(hdr.Name));
        while (!name.empty() && name.back() == ' ') {
            name.pop_back();
        }
        if (name.empty()) {
            if (verbose) std::cout << "   name raw empty\n";
            return {};
        }

        if (name == "/")  return "/";
        if (name == "//") return "//";

        if ((name.size() > 1) && name.front() == '/' && std::isdigit(static_cast<unsigned char>(name[1]))) {
            auto const offset = static_cast<std::size_t>(std::strtoul(name.c_str() + 1, nullptr, 10));
            if (offset < longnames.size()) {
                auto tail = longnames.substr(offset);
                auto end  = tail.find('/');
                return std::string(tail.substr(0, end));
            }
            if (verbose) std::cout << "   longname offset out of range: off=" << offset << " longnames=" << longnames.size() << "\n";
            return {};
        }

        if (!name.empty() && name.back() == '/') {
            name.pop_back();
        }
        return name;
    }

    using SymbolsByOffset = std::unordered_multimap<std::uint32_t, std::string>;

    SymbolsByOffset parse_linker_member(std::string_view member, bool verbose) {
        SymbolsByOffset map;
        if (member.size() < 4) {
            if (verbose) std::cout << " ! linker member too small\n";
            return map;
        }
        auto const n_symbols = read_be_u32(member.data());
        auto const offsets_bytes = static_cast<std::size_t>(n_symbols) * 4;
        if (4 + offsets_bytes >= member.size()) {
            if (verbose) std::cout << " ! linker member offsets out of range\n";
            return map;
        }
        auto const* offsets_base = member.data() + 4;
        auto const* strings_base = offsets_base + offsets_bytes;
        auto const* strings_end  = member.data() + member.size();
        std::size_t sym_idx = 0;
        auto p = strings_base;
        while ((p < strings_end) && (sym_idx < n_symbols)) {
            auto const off = read_be_u32(offsets_base + sym_idx * 4);
            auto const len = std::strlen(p);
            map.emplace(off, std::string(p, len));
            p += len + 1;
            ++sym_idx;
        }
        if (verbose) std::cout << " linker member symbols parsed: " << map.size() << "\n";
        return map;
    }

    bool parse_coff_object(std::string_view member, std::string const& lib_name, db::Storage& storage, bool verbose, std::string const& member_name) {
        auto const need = std::min<std::size_t>(member.size(), sizeof(ANON_OBJECT_HEADER_BIGOBJ));
        if (need < sizeof(IMAGE_FILE_HEADER)) {
            if (verbose) std::cout << " ! skip (too small for header)\n";
            return false;
        }

        bool   is_bigobj = false;
        bool   is_import = false;
        WORD   sig1      = 0;
        WORD   sig2      = 0;
        std::memcpy(&sig1, member.data(), sizeof(sig1));
        std::memcpy(&sig2, member.data() + sizeof(sig1), sizeof(sig2));
        if (sig1 == 0 && sig2 == 0xFFFF) {
            if (member.size() >= sizeof(IMPORT_OBJECT_HEADER)) {
                IMPORT_OBJECT_HEADER ih{};
                std::memcpy(&ih, member.data(), sizeof(ih));
                if (ih.Version <= 1 && ih.SizeOfData <= member.size()) {
                    is_import = true;
                }
            }
            if (!is_import && member.size() >= sizeof(ANON_OBJECT_HEADER_BIGOBJ)) {
                is_bigobj = true;
            }
        }

        if (is_import) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (import object)\n";
            return false;
        }

        std::size_t n_symbols   = 0;
        std::size_t sym_offset  = 0;

        if (is_bigobj) {
            ANON_OBJECT_HEADER_BIGOBJ hdr{};
            std::memcpy(&hdr, member.data(), sizeof(hdr));
            n_symbols  = hdr.NumberOfSymbols;
            sym_offset = hdr.PointerToSymbolTable;
        } else {
            IMAGE_FILE_HEADER hdr{};
            std::memcpy(&hdr, member.data(), sizeof(hdr));
            n_symbols  = hdr.NumberOfSymbols;
            sym_offset = hdr.PointerToSymbolTable;
        }

        if ((sym_offset == 0) || (n_symbols == 0)) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (no symbol table)\n";
            return false;
        }

        auto const symbols_offset = static_cast<std::size_t>(sym_offset);
        auto const symbols_bytes  = static_cast<std::size_t>(n_symbols) * sizeof(IMAGE_SYMBOL);

        if (verbose) {
            std::cout << " > member [" << member_name << "] size=" << member.size()
                      << " bigobj=" << is_bigobj << " sym_off=" << symbols_offset
                      << " n_sym=" << n_symbols << " sym_bytes=" << symbols_bytes << "\n";
        }

        if (symbols_offset > member.size()) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (symbol table offset out of range: off="
                                   << symbols_offset << " size=" << member.size() << ")\n";
            return false;
        }
        if (n_symbols > (member.size() - symbols_offset) / sizeof(IMAGE_SYMBOL)) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (symbol table truncated: off="
                                   << symbols_offset << " n=" << n_symbols << " member=" << member.size() << ")\n";
            return false;
        }

        auto const* symbols      = reinterpret_cast<IMAGE_SYMBOL const*>(member.data() + symbols_offset);
        auto const* string_table = reinterpret_cast<char const*>(symbols) + symbols_bytes;
        if ((string_table + sizeof(std::uint32_t)) > (member.data() + member.size())) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (string table header out of range)\n";
            return false;
        }

        auto const str_size = *reinterpret_cast<std::uint32_t const*>(string_table);
        if (str_size < sizeof(std::uint32_t)) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (bad string table size: " << str_size << ")\n";
            return false;
        }
        auto const str_hdr_off = static_cast<std::size_t>(string_table - member.data());
        if ((string_table + str_size) > (member.data() + member.size())) {
            if (verbose) std::cout << " ! skip [" << member_name << "] (string table truncated: hdr_off="
                                   << str_hdr_off << " str_size=" << str_size << " member=" << member.size() << ")\n";
            return false;
        }
        auto const* str_base = string_table + sizeof(std::uint32_t);

        for (std::uint32_t i = 0; i < n_symbols; ++i) {
            auto const& sym = symbols[i];

            if ((sym.StorageClass == IMAGE_SYM_CLASS_EXTERNAL) && (sym.SectionNumber > 0)) {
                std::string mangled;

                if (sym.N.ShortName[0] != 0) {
                    auto const* short_name = reinterpret_cast<char const*>(sym.N.ShortName);
                    mangled.assign(short_name, short_len(short_name, sizeof(sym.N.ShortName)));
                } else {
                    auto const offset = sym.N.Name.Long;
                    if (offset < str_size) {
                        auto const* start = str_base + offset;
                        mangled.assign(start, short_len(start, str_size - offset));
                    }
                }

                if (!mangled.empty()) {
                    auto const pretty = demangle_verbose(mangled, verbose);
                    storage.insert(db::Symbol{ lib_name, pretty, mangled, pretty });
                    if (verbose) {
                        std::cout << " + " << lib_name << ": " << pretty << " (" << mangled
                                  << ") from [" << member_name << "]\n";
                    }
                }
            }

            i += sym.NumberOfAuxSymbols;
        }

        return true;
    }

    void build_index_windows(fs::path const& path, db::Storage& storage, bool verbose) {
        std::cout << path << std::endl;

        auto const data = read_file(path);
        bool const is_thin = std::string_view(data).starts_with(ARCHIVE_THIN_MAGIC);
        if (!is_thin && !std::string_view(data).starts_with(ARCHIVE_MAGIC)) {
            throw std::runtime_error(std::format("'{}' is not a COFF archive", path.string()));
        }

        auto const& lib_name = path.stem().string();
        std::string_view blob{ data };
        std::string_view longnames;
        SymbolsByOffset  linker_symbols;
        std::size_t      offset = is_thin ? ARCHIVE_THIN_MAGIC.size() : ARCHIVE_MAGIC.size();

        while ((offset + sizeof(IMAGE_ARCHIVE_MEMBER_HEADER)) <= blob.size()) {
            auto const& hdr = *reinterpret_cast<IMAGE_ARCHIVE_MEMBER_HEADER const*>(blob.data() + offset);
            auto const header_offset = offset;
            offset += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);

            auto const size = static_cast<std::size_t>(std::strtoull(reinterpret_cast<char const*>(hdr.Size), nullptr, 10));
            if ((offset + size) > blob.size()) {
                throw std::runtime_error("Corrupt archive member size");
            }

            std::string_view member{ blob.data() + offset, size };
            offset += size;
            if ((offset % 2) != 0) {
                ++offset;
            }

            auto const name = resolve_member_name(hdr, longnames, verbose);
            if (verbose) {
                std::ostringstream raw_hex;
                raw_hex << std::hex << std::setfill('0');
                for (auto ch : std::string_view(reinterpret_cast<char const*>(hdr.Name), sizeof(hdr.Name))) {
                    raw_hex << std::setw(2) << (static_cast<unsigned>(static_cast<unsigned char>(ch))) << ' ';
                }
                std::cout << " member rawName=[" << raw_hex.str() << "] size=" << size << " resolved=\"" << name << "\"\n";
            }

            if (name == "/") {
                linker_symbols = parse_linker_member(member, verbose);
                continue;
            }
            if (name == "//") {
                longnames = member;
                continue;
            }
            if (name.empty()) {
                if (verbose) std::cout << " ! skip (empty name)\n";
                continue;
            }

            if (is_thin) {
                fs::path ext = path.parent_path() / name;
                if (!fs::exists(ext)) {
                    if (verbose) {
                        std::cout << " ! skip (thin member missing) " << ext.string() << "\n";
                    }
                    continue;
                }
                auto ext_data = read_file(ext);
                if (!parse_coff_object(std::string_view{ ext_data }, lib_name, storage, verbose, name)) {
                    auto range = linker_symbols.equal_range(static_cast<std::uint32_t>(header_offset));
                    for (auto it = range.first; it != range.second; ++it) {
                        auto const& mangled = it->second;
                        auto const  pretty  = demangle_verbose(mangled, verbose);
                        storage.insert(db::Symbol{ lib_name, pretty, mangled, pretty });
                        if (verbose) std::cout << " + " << lib_name << ": " << pretty << " (from linker member)\n";
                    }
                }
            } else {
                if (!parse_coff_object(member, lib_name, storage, verbose, name)) {
                    auto range = linker_symbols.equal_range(static_cast<std::uint32_t>(header_offset));
                    for (auto it = range.first; it != range.second; ++it) {
                        auto const& mangled = it->second;
                        auto const  pretty  = demangle_verbose(mangled, verbose);
                        storage.insert(db::Symbol{ lib_name, pretty, mangled, pretty });
                        if (verbose) std::cout << " + " << lib_name << ": " << pretty << " (from linker member)\n";
                    }
                }
            }
        }
    }
#endif

}  // anonymous namespace

// Exported function implementations
namespace indexer {

    std::string demangle(std::string const& mangled, bool verbose) {
#if __linux__
        // On Linux, use cxxabi or return as-is
        return mangled;  // TODO: implement with abi::__cxa_demangle
#else
        if (mangled.size() >= 2 && mangled[0] == '_' && mangled[1] == 'Z') {
            if (verbose) std::cout << "     [demangle] detected Itanium mangling (_Z prefix)\n";
            if (auto ita = try_demangle_itanium(mangled, verbose); !ita.empty()) {
                return ita;
            }
            if (auto simple = try_demangle_itanium_names_only(mangled); !simple.empty()) {
                if (verbose) std::cout << "     [demangle] fallback names_only: " << simple << "\n";
                return simple;
            }
            if (verbose) std::cout << "     [demangle] Itanium parsing failed, returning as-is\n";
            return mangled;
        }

        char buffer[1024];
        auto const len = ::UnDecorateSymbolName(mangled.c_str(), buffer, static_cast<DWORD>(std::size(buffer)), UNDNAME_COMPLETE);
        if (len != 0) {
            std::string result(buffer, len);
            if (result != mangled) {
                if (verbose) std::cout << "     [demangle] MSVC: " << result << "\n";
                return result;
            }
        }

        if (verbose) std::cout << "     [demangle] no demangling applied\n";
        return mangled;
#endif
    }

    void build_index(fs::path const& path, db::Storage& storage, bool verbose) {
#if __linux__
        std::cout << path << std::endl;

        try {
            BfdWrapper lib{ path.string() };

            for (auto const& bfd : BfdRange(lib)) {
                auto const& library{ bfd.filename() };
                for (auto const member : bfd.symbols()) {
                    std::string_view mangled{ member->name };
                    auto             demangled{ bfd.demangle(mangled) };
                    auto             symbol{ extract_symbol(demangled) };

                    storage.insert(db::Symbol{ library, String(symbol), String(mangled), String(demangled) });
                    if (verbose) {
                        std::cout << " + " << library << ": " << demangled << " (" << mangled << ")\n";
                    }
                };
            }
        }
        catch (...) {
            std::rethrow_exception(std::current_exception());
        }
#else
        build_index_windows(path, storage, verbose);
#endif
    }

}  // namespace indexer