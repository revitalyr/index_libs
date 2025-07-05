// work.cpp

import std;
#include <cassert>
#include <cstdlib>

namespace fs = std::filesystem;

import args_module;
import types;
import storage_module;
#if __linux__
import bfd_wrapper_module;
#else
#endif

using LibPath   = fs::path;
using ArgPath   = args::ValueFlag<std::string, args::ValueReader>;
using ArgSymbol = args::Positional<std::string>;

struct Indexer {
    LibPath m_libpath;
};

namespace {
#if __linux__
    StrView lib_extension{ ".a" };

    void build_index (fs::path const &path) {
        std::cout << path << std::endl;

        try {
            db::Storage storage;

            for (auto const &bfd : BfdRange(BfdWrapper{ path })) {
                auto const &library{ bfd.filename() };
                for (auto const member : bfd.symbols()) {
                    std::string_view mangled{ member->name };
                    auto             demangled{ bfd.demangle(name) };
                    auto             symbol{ demangled };  // TO_DO

                    storage.insert(db::Symbol{ library, symbol, mangled, demangled });
                };
            }
        } catch (...) {
            rethrow(__func__, path);
        }
#else
    StrView          lib_extension{ ".lib" };
    std::regex const RE{ R"(^[0-9A-F]{3} .{29}External[^?]*\?([^ ]+) (.+))" };

    void build_index (fs::path const &path) {
        std::cout << path << std::endl;

        try {
            auto               line_no{ 0 };
            auto const        &lib_name{ path.stem().string() };
            std::istringstream input{ get_process_output("dumpbin.exe", { "/SYMBOLS", path.string() }) };

            for (std::string line; std::getline(input, line);) {
                std::smatch fields;

                if (line.starts_with("LINK : fatal error"))
                    throw std::runtime_error(line);

                if (std::regex_search(line, fields, RE) && (fields.size() == 3)) {
                    try {
                        auto mangled{ fields[1].str() };
                        auto unmangled{ fields[2].str() };
                        // std::println("{} -> {}", mangled, unmangled);
                        db::Symbol symbol{ lib_name, mangled, unmangled };
                        // std::cout << symbol << '\n';
                        std::cout << '\r' << line_no++;
                        storage.insert(symbol);
                    } catch (...) {
                        rethrow(__func__, line);
                    }
                }
            }
        } catch (...) {
            rethrow(__func__, path);
        }
    }
#endif
    }  // namespace

    int main (int argc, char *argv[]) {
        args::ArgumentParser parser("Indexing COFF archives contents and searching archives by a given symbol");
        args::HelpFlag       help(parser, "help", "Display this help menu", { 'h', "help" }, {});
        args::Group          group(parser, "This arguments are exclusive:", args::Group::Validators::Xor);
        ArgPath              lib_path(group, "BUILD", "Path to the library (or directory of libraries) for which the index will be built", { 'b', "build" });
        ArgSymbol            symbol_arg(group, "SYMBOL", "Symbol to find");
        auto                 args_ok = false;

        try {
            parser.helpParams.width = 120;
            parser.ParseCLI(argc, argv);

            if (lib_path) {
                auto path = args::get(lib_path);

                if (!fs::exists(path)) {
                    throw std::runtime_error(std::format("'{}' does not exist", path));
                }

                args_ok = true;

                auto f_type = fs::status(path).type();
                switch (f_type) {
                    case fs::file_type::regular:
                        build_index(path, storage);
                        break;
                    case fs::file_type::directory: {
                        for (fs::directory_entry const &entry : fs::recursive_directory_iterator(path)) {
                            if (entry.path().extension() == lib_extension) {
                                build_index(entry, storage);
                            }
                        }
                        break;
                    }
                    default:
                        throw std::runtime_error(std::format("'{}' has wrong type {}", path, static_cast<int>(f_type)));
                }
            }

            if (symbol_arg) {
                auto const &symbol = args::get(symbol_arg);
                auto const  libs{ storage.find(symbol) };

                for (auto const &lib : libs) {
                    std::cout << lib << '\n';
                }
            }

        } catch (std::exception const &e) {
            std::cerr << typeid(e).name() << ": " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }