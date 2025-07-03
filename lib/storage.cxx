export module Storage;

import std;
import sqlite_orm_module;

namespace db {
    using String  = std::string;
    using Strings = std::vector<String> const;

    export struct Symbol {
        String m_library;
        String m_mangled;
        String m_unmangled;
        String m_symbol;

        Symbol() = default;

        Symbol (String const &library, String const &symbol, String const &mangled, String const &unmangled)
            : m_library(library), m_symbol(symbol), m_mangled(mangled), m_unmangled(unmangled) {
        }
    };

    using sqlite_orm::c;
    using sqlite_orm::columns;
    using sqlite_orm::indexed_column;
    using sqlite_orm::length;
    using sqlite_orm::like;
    using sqlite_orm::make_column;
    using sqlite_orm::make_index;
    using sqlite_orm::make_storage;
    using sqlite_orm::make_table;
    using sqlite_orm::make_unique_index;
    using sqlite_orm::select;
    using sqlite_orm::where;

    using sqlite_orm::make_error_code;

    using sqlite_orm::arithmetic_t;
    using sqlite_orm::greater_than_t;
    using sqlite_orm::is_operator_argument;
    using sqlite_orm::unwrap_expression_t;
    using sqlite_orm::internal::expression_t;

    //    using sqlite_orm::internal::statement_serializer;

    std::basic_ostream<char> &operator<<(std::basic_ostream<char> &ostr, Symbol const &symbol) {
        return ostr << std::format("{}, {}, {}", symbol.m_library, symbol.m_symbol, symbol.m_mangled);
    }

    auto storage = make_storage(
        "db/libraries.db",
        make_index("idx_symbol", &Symbol::m_symbol),
        make_table("content", make_column("library", &Symbol::m_library), make_column("mangled", &Symbol::m_mangled), make_column("m_unmangled", &Symbol::m_unmangled), make_column("symbol", &Symbol::m_symbol))
    );

    export class Storage {
       public:
        Storage () {
            storage.sync_schema();
        }

        ~Storage() = default;

        void insert (Symbol const &symbol) {
            if (!symbol.m_symbol.empty()) {
                storage.insert(symbol);
            }
        }

        Strings find (String const &symbol) {
            std::vector<String> result;
            auto                rows = storage.select(columns(&Symbol::m_library, &Symbol::m_unmangled), where(like(&Symbol::m_symbol, "%" + symbol + "%")));

            result.reserve(rows.size());
            for (auto const &row : rows) {
                result.push_back(std::format("{}: {}", std::get<0>(row), std::get<1>(row)));
            }

            return result;
        }
    };
}  // namespace db