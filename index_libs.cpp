// index_libs.cpp : parse library's content and put it into database

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define TERMCOLOR_USE_ANSI_ESCAPE_SEQUENCES
#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

import std;
#include <cassert>
#include <cstdlib>

import args_module;
import termcolor_module;
import sqlite_orm_module;
import boost_module;    //get_process_output

namespace fs = std::filesystem;
namespace ranges = std::ranges;

using namespace std::literals;

//https://stackoverflow.com/questions/37227300/why-doesnt-c-use-stdnested-exception-to-allow-throwing-from-destructor
// this function will re-throw the current exception, nested inside a
// new one. If the std::current_exception is derived from logic_error, 
// this function will throw a logic_error. Otherwise it will throw a
// runtime_error
// The message of the exception will be composed of the arguments
// context and the variadic arguments args... which may be empty.
// The current exception will be nested inside the new one
// @pre context and args... must support ostream operator <<
template<class Context, class...Args>
void rethrow(Context &&context, Args&&... args) {
    // build an error message
    std::ostringstream ss;
    ss << termcolor::colorize << termcolor::red << context << termcolor::reset;
    auto sep = ": ";
    using expand = int[];
    void(expand{ 0, ((ss << sep << args), sep = " ", 0)... });
    // figure out what kind of exception is active
    try {
        std::rethrow_exception(std::current_exception());
    }
    catch (const std::invalid_argument &e) {
        std::throw_with_nested(std::invalid_argument(ss.str()));
    }
    catch (...) {
        std::throw_with_nested(std::runtime_error(ss.str()));
    }
}

// unwrap nested exceptions, printing each nested exception to 
// std::cerr
void print_exception(const std::exception &e, std::size_t depth = 0) {
    std::cerr << "exception: " << std::string(depth, ' ') << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    }
    catch (const std::exception &nested) {
        print_exception(nested, depth + 1);
    }
}

namespace {
    std::string const extract_symbol(const std::string_view &line) {
        auto const str = line.substr(1, line.length() - 2);
        auto const opening_parenthesis_pos = str.find_last_of('(');
        std::string symbol;

        if ((opening_parenthesis_pos == ""sv.npos)) {
            auto const blank_pos = str.find_last_of(' ');
            try
            {
                assert(blank_pos != ""sv.npos);
                symbol = str.substr(blank_pos + 1, str.length() - blank_pos - 1);
                //std::println("{}\n{}", str, symbol);
            }
            catch (...) {
                rethrow(__func__, "opening_parenthesis_pos: ", opening_parenthesis_pos, ", blank_pos: ", blank_pos, str);
            }
        } else {
            try
            {
                auto s{ str.substr(0, opening_parenthesis_pos) };
                symbol = ranges::to<std::string>(ranges::find_last(s, ' ')).substr(1);
                ///std::println("{}\n{}", str, symbol);
            }
            catch (...) {
                rethrow(__func__, "opening_parenthesis_pos: ", opening_parenthesis_pos, str);
            }
        }

        return symbol;
    }
}

namespace db {
    using sqlite_orm::length;
    using sqlite_orm::make_storage;
    using sqlite_orm::where;
    using sqlite_orm::make_index;
    using sqlite_orm::indexed_column;
    using sqlite_orm::make_unique_index;
    using sqlite_orm::make_column;
    using sqlite_orm::make_table;
    using sqlite_orm::c;

    using sqlite_orm::make_error_code;

    using sqlite_orm::greater_than_t;
    using sqlite_orm::arithmetic_t;
    using sqlite_orm::is_operator_argument;
    using sqlite_orm::unwrap_expression_t;

    struct Symbol {
        std::string m_library;
        std::string m_mangled;
        std::string m_unmangled;
        std::string m_symbol;
    public:
        Symbol() = default;
        Symbol(std::string const &library, std::string const &mangled, std::string const &unmangled)
            : m_library(library), m_mangled(mangled), m_unmangled(unmangled) {
            try
            {
                m_symbol = extract_symbol(unmangled);
            }
            catch (...) {
                rethrow(__func__, "library:", library, ", mangled:", mangled, ", unmangled:", unmangled);
            }
        }
    };

    auto storage = make_storage(
        "db/libraries.db",
        make_index("idx_symbol", &Symbol::m_symbol),
        make_table("content",
                   make_column("library", &Symbol::m_library),
                   make_column("mangled", &Symbol::m_mangled),
                   make_column("m_unmangled", &Symbol::m_unmangled),
                   make_column("symbol", &Symbol::m_symbol)
        )
    );
}

std::basic_ostream<char> &operator<<(std::basic_ostream<char> &ostr, db::Symbol const &symbol) {
    return ostr << std::format("{}, {}, {}", symbol.m_library, symbol.m_symbol, symbol.m_unmangled);
}


namespace {
    const std::regex    RE{ R"(^[0-9A-F]{3} .{29}External[^?]*\?([^ ]+) (.+))" };

    void process_file(fs::path const &path) {
        std::cout << path << std::endl;

        try
        {
            auto const &lib_name{ path.stem().string() };
            std::istringstream  input{ get_process_output("dumpbin.exe", { "/SYMBOLS", path.string() })};

            for (std::string line; std::getline(input, line);) {
                std::smatch fields;

                if (line.starts_with("LINK : fatal error"))
                    throw std::runtime_error(line);

                if (std::regex_search(line, fields, RE) && (fields.size() == 3)) {
                    try {
                        auto mangled{ fields[1].str() };
                        auto unmangled{ fields[2].str() };
                        //std::println("{} -> {}", mangled, unmangled);
                        db::Symbol  symbol{ lib_name, mangled, unmangled };
                        std::cout << symbol << '\n';
                        db::storage.insert(symbol);
                    }
                    catch (...) {
                        rethrow(__func__, line);
                    }
                }
            }
        }
        catch (...) {
            rethrow(__func__, path);
        }
    }
}

#define DUMP_IF_OK(symbol)  if(symbol) { std::cout << "found " << #symbol << " = '" << args::get(symbol) << "'\n"; }

using ArgPath = args::ValueFlag<std::string, args::ValueReader>;
using ArgSymbol = args::Positional<std::string>;


int main(int argc, char **argv) {
    args::ArgumentParser    parser("args parser");
    args::HelpFlag          help(parser, "help", "Display this help menu", { 'h', "help" }, {});
    args::Group             group(parser, "This arguments are exclusive:", args::Group::Validators::Xor);
    ArgPath                 lib_path(group, "BUILD", "Path to the library for which the index is built", { 'b', "build" });
    ArgSymbol               symbol_arg(group, "SYMBOL", "Symbol to find");
    auto                args_ok = false;

    try {
        parser.helpParams.width = 120;
        parser.ParseCLI(argc, argv);

        db::storage.sync_schema();

        if (lib_path) {
            auto path = args::get(lib_path);

            if (!fs::exists(path)) {
                throw std::runtime_error(std::format("'{}' does not exist", path));
            }

            args_ok = true;

            auto f_type = fs::status(path).type();
            switch (f_type) {
                case fs::file_type::regular:
                    process_file(path);
                    break;
                default:
                    throw std::runtime_error(std::format("'{}' has wrong type {}", path, static_cast<int>(f_type)));
            }
        }

        if (symbol_arg) {
            using sqlite_orm::like;
            auto const & symbol = args::get(symbol_arg);
            auto libs = db::storage.get_all<db::Symbol>(where(like(&db::Symbol::m_unmangled),symbol));
            //std::cout << "'" << symbol << "' found in " << db::storage.dump(symbol) << std::endl;
            //std::println("{}", libs);
            for (auto const &lib : libs) {
                std::cout << lib << '\n';
            }
        }
    }
    catch (args::Help) {
        std::cout << parser;
        return EXIT_SUCCESS;
    }
    catch (args::ValidationError) {
        auto const & args = *static_cast<args::Group const *> (parser.Children()[1]);
        if (args::Group::Validators::None(args)) {
            std::cerr << "\n\n  There are missed or the SYMBOL or the BUILD argument.\n\n" << parser;
        } else {
            std::cerr << "\n\n  The SYMBOL and BUILD arguments are mutually exclusive.\n\n" << parser;
        }

        return EXIT_FAILURE;
    }
    catch (std::exception const & e) {
        //std::cerr << termcolor::red << typeid(e).name() << ": " << e.what() << termcolor::reset << std::endl;
        print_exception(e);
        if(!args_ok) {
            std::cerr << parser;
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
