// main.cpp
//
// Symbol search format:
//   The symbol argument is a substring to search for. The search matches any symbol
//   or unmangled name that contains the given substring (case-sensitive).
//   Examples:
//     printf         - finds all symbols containing "printf"
//     std::vector    - finds all symbols containing "std::vector"
//     operator new   - finds all symbols containing "operator new"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <typeinfo>

import std;

namespace fs = std::filesystem;

import args_module;
import types;
import storage_module;
import indexer_module;

namespace {
    inline std::string default_db_path() {
        return (fs::temp_directory_path() / "db/libraries_unqlite.db").string();
    }

    struct ParsedArgs {
        std::string build_path;
        std::string symbol;
        std::string db_path;
        std::string export_path;
        bool do_export = false;
        bool verbose = false;

        bool has_build_path() const { return !build_path.empty(); }
        bool has_symbol() const { return !symbol.empty(); }

        void print(std::ostream& os) const {
            os << "Parsed arguments:\n";
            os << "  build path: " << (has_build_path() ? build_path : "<not specified>") << '\n';
            os << "  symbol:     " << (has_symbol() ? symbol : "<not specified>") << '\n';
            os << "  database:   " << db_path << '\n';
            os << "  export:     " << (do_export ? (export_path.empty() ? "<stdout>" : export_path) : "<not specified>") << '\n';
            os << "  verbose:    " << (verbose ? "true" : "false") << '\n';
            os << '\n';
        }

        static void print_error_and_help(std::ostream& os, const std::string& error_msg, const args::ArgumentParser& parser, int argc, char* argv[]) {
            os << "Error: " << error_msg << "\n\nCommand line: ";
            for (int i = 0; i < argc; ++i) {
                if (i > 0) os << ' ';
                os << argv[i];
            }
            os << "\n\nusing:\n" << parser.Help();
        }

        static std::string translate_parse_error(const std::string& msg) {
            if (msg.find("no positional arguments were ready to receive") != std::string::npos) {
                auto pos = msg.rfind(':');
                std::string extra = (pos != std::string::npos) ? msg.substr(pos + 2) : "";
                return "Unexpected argument: '" + extra + "'\n"
                       "The -v/--verbose flag doesn't take a value. Use just -v without arguments.";
            }
            else if (msg.find("Flag could not be matched") != std::string::npos) {
                auto pos = msg.rfind(':');
                std::string flag = (pos != std::string::npos) ? msg.substr(pos + 2) : "";
                return "Unknown option: '" + flag + "'";
            }
            return msg;
        }

        static ParsedArgs parse(int argc, char* argv[]) {
            args::ArgumentParser parser("Indexing COFF archives contents and searching archives by a given symbol");
            args::HelpFlag       help(parser, "help", "Display this help menu", { 'h', "help" });
            args::ValueFlag<std::string, args::ValueReader> lib_path(parser, "path", "Path to the library (or directory of libraries) for which the index will be built", { 'b', "build" });
            args::ValueFlag<std::string, args::ValueReader> db_path_arg(parser, "path", "Path to the database file", { 'd', "database" });
            args::ValueFlag<std::string, args::ValueReader> export_arg(parser, "file", "Export database to CSV file (use - for stdout)", { 'e', "export" }, args::Options::Single);
            args::Flag export_flag(parser, "export", "Export database to stdout as CSV", { 'E' });
            // Symbol search: substring match (case-sensitive). Examples: "printf", "std::vector", "operator new"
            args::Positional<std::string> symbol_arg(parser, "symbol", 
                "Symbol substring to find (case-sensitive). Matches any symbol or unmangled name containing this text. "
                "Examples: 'printf', 'std::vector', 'operator new'");
            args::Flag verbose_flag(parser, "verbose", "Print symbols while indexing", { 'v', "verbose" });

            parser.helpParams.width = 120;

            if (argc <= 1) {
                print_error_and_help(std::cerr, "No arguments provided. Use -b/--build <path> to index archives and/or provide a <symbol> to search.", parser, argc, argv);
                throw std::invalid_argument("No arguments provided");
            }

            try {
                parser.ParseCLI(argc, argv);
            }
            catch (args::Help const&) {
                std::cout << parser;
                throw;
            }
            catch (args::ParseError const& e) {
                print_error_and_help(std::cerr, translate_parse_error(e.what()), parser, argc, argv);
                throw;
            }
            catch (args::ValidationError const& e) {
                print_error_and_help(std::cerr, e.what(), parser, argc, argv);
                throw;
            }

            ParsedArgs result;
            result.verbose = verbose_flag;
            if (lib_path) {
                result.build_path = args::get(lib_path);
            }
            if (symbol_arg) {
                result.symbol = args::get(symbol_arg);
            }
            result.db_path = db_path_arg ? args::get(db_path_arg) : default_db_path();

            if (export_flag) {
                result.do_export = true;
            }
            if (export_arg) {
                result.do_export = true;
                auto path = args::get(export_arg);
                if (path != "-") {
                    result.export_path = path;
                }
            }

            if (!result.has_build_path() && !result.has_symbol() && !result.do_export) {
                print_error_and_help(std::cerr, "No action specified. Use -b/--build <path> to index archives, provide a <symbol> to search, or use -e/--export to export database.", parser, argc, argv);
                throw std::invalid_argument("No action specified");
            }

            return result;
        }
    };
}

int main(int argc, char* argv[]) {
    try {
        auto args = ParsedArgs::parse(argc, argv);

        if (args.verbose) {
            args.print(std::cout);
        }

        db::Storage storage(args.db_path);

        if (args.has_build_path()) {
            auto const& path = args.build_path;

            if (!fs::exists(path)) {
                throw std::runtime_error(std::format("'{}' does not exist", path));
            }

            auto f_type = fs::status(path).type();
            switch (f_type) {
            case fs::file_type::regular:
                indexer::build_index(path, storage, args.verbose);
                break;
            case fs::file_type::directory: {
                for (fs::directory_entry const& entry : fs::recursive_directory_iterator(path)) {
                    if (entry.path().extension() == indexer::lib_extension) {
                        indexer::build_index(entry, storage, args.verbose);
                    }
                }
                break;
            }
            default:
                throw std::runtime_error(std::format("'{}' has wrong type {}", path, static_cast<int>(f_type)));
            }
        }

        if (args.has_symbol()) {
            std::cout << "Searching for '" << args.symbol << "'...\n";
            
            auto const libs{ storage.find(args.symbol) };

            if (libs.empty()) {
                std::cout << "No matches found.\n";
            } else {
                std::cout << "Found " << libs.size() << " match(es):\n";
                for (auto const& lib : libs) {
                    std::cout << "  " << lib << '\n';
                }
            }
        }

        if (args.do_export) {
            if (args.export_path.empty()) {
                storage.export_csv(std::cout);
            } else {
                std::ofstream out(args.export_path);
                if (!out) {
                    throw std::runtime_error(std::format("Cannot open '{}' for writing", args.export_path));
                }
                storage.export_csv(out);
                std::cout << "Exported to '" << args.export_path << "'\n";
            }
        }

    }
    catch (args::Help const&) {
        return EXIT_SUCCESS;
    }
    catch (std::exception const& e) {
        if (!dynamic_cast<args::Error const*>(&e)) {
            std::cerr << typeid(e).name() << ": " << e.what() << std::endl;
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}