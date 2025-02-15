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

#include "utils.h"
#include "storage.h"


namespace {
    const std::regex    RE{ R"(^[0-9A-F]{3} .{29}External[^?]*\?([^ ]+) (.+))" };

    void build_index(fs::path const &path, db::Storage &storage) {
        std::cout << path << std::endl;

        try
        {
            auto line_no{ 0 };
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
                        //std::cout << symbol << '\n';
                        std::cout << '\r' << line_no++;
                        storage.insert(symbol);
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

        db::Storage storage;
        

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
                    for (const fs::directory_entry &entry : fs::recursive_directory_iterator(path)) {
                        if(entry.path().extension() == ".lib") {
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
            auto const & symbol = args::get(symbol_arg);
            auto const libs{ storage.find(symbol)};

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
