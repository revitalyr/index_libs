module;

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <format>
#include <tuple>
#include <ostream>
#include <memory>
#include <iostream>

export module storage_module;

import unqlite_module;

namespace db {
    using String  = std::string;
    using Strings = std::vector<String>;

    export struct Symbol {
        String m_library;
        String m_symbol;
        String m_mangled;
        String m_unmangled;

        Symbol() = default;

        Symbol(String const &library, String const &symbol, String const &mangled, String const &unmangled)
            : m_library(library), m_symbol(symbol), m_mangled(mangled), m_unmangled(unmangled) {
        }
    };

    std::basic_ostream<char> &operator<<(std::basic_ostream<char> &ostr, Symbol const &symbol) {
        return ostr << std::format("{}, {}, {}", symbol.m_library, symbol.m_symbol, symbol.m_mangled);
    }

    // Escape a string for CSV (RFC 4180)
    String csv_escape(String const& s) {
        bool needs_quotes = s.find_first_of(",\"\r\n") != String::npos;
        if (!needs_quotes) {
            return s;
        }
        String result = "\"";
        for (char c : s) {
            if (c == '"') {
                result += "\"\"";
            } else {
                result += c;
            }
        }
        result += "\"";
        return result;
    }

    class StorageImpl {
        unqlite* m_db{nullptr};
        String   m_db_path;

        void ensure_initialized_for_write() {
            if (!m_db) {
                auto db_dir = std::filesystem::path(m_db_path).parent_path();
                if (!db_dir.empty() && !std::filesystem::exists(db_dir)) {
                    std::filesystem::create_directories(db_dir);
                }
                
                int rc = unqlite_open(&m_db, m_db_path.c_str(), 
                    unqlite_flags::OPEN_CREATE | unqlite_flags::OPEN_READWRITE | unqlite_flags::OPEN_OMIT_JOURNALING);
                
                if (rc != unqlite_flags::OK) {
                    throw std::runtime_error(std::format("Failed to open database '{}' for writing", m_db_path));
                }
            }
        }

        void ensure_initialized_for_read() {
            if (!m_db) {
                if (!std::filesystem::exists(m_db_path)) {
                    throw std::runtime_error(std::format("Database '{}' does not exist", m_db_path));
                }
                
                int rc = unqlite_open(&m_db, m_db_path.c_str(), unqlite_flags::OPEN_READONLY);
                
                if (rc != unqlite_flags::OK) {
                    throw std::runtime_error(std::format("Failed to open database '{}' for reading", m_db_path));
                }
            }
        }

    public:
        explicit StorageImpl(String const &db_path) : m_db_path(db_path) {}

        ~StorageImpl() {
            if (m_db) {
                unqlite_close(m_db);
            }
        }

        void insert(Symbol const &symbol) {
            if (!symbol.m_symbol.empty()) {
                ensure_initialized_for_write();
                
                // Key: library|mangled
                // Value: unmangled|symbol
                String key = std::format("{}|{}", symbol.m_library, symbol.m_mangled);
                String value = std::format("{}|{}", symbol.m_unmangled, symbol.m_symbol);

                unqlite_kv_store(m_db, key.data(), (int)key.size(), value.data(), (int)value.size());
            }
        }

        Strings find(String const &query) {
            ensure_initialized_for_read();
            Strings result;

            unqlite_kv_cursor* cursor;
            int rc = unqlite_kv_cursor_init(m_db, &cursor);
            if (rc != unqlite_flags::OK) return result;

            // Iterate over all records (Linear scan for LIKE functionality)
            for (unqlite_kv_cursor_first_entry(cursor); unqlite_kv_cursor_valid_entry(cursor); unqlite_kv_cursor_next_entry(cursor)) {
                
                // Fetch Key
                int nKeyLen = 0;
                unqlite_kv_cursor_key(cursor, nullptr, &nKeyLen);
                String key(nKeyLen, 0);
                unqlite_kv_cursor_key(cursor, key.data(), &nKeyLen);

                // Fetch Value
                unqlite_int64 nDataLen = 0;
                unqlite_kv_cursor_data(cursor, nullptr, &nDataLen);
                String value(nDataLen, 0);
                unqlite_kv_cursor_data(cursor, value.data(), &nDataLen);

                // Parse Key: library|mangled
                auto sep_key = key.find('|');
                String lib = (sep_key != String::npos) ? key.substr(0, sep_key) : "unknown";
                String mangled = (sep_key != String::npos) ? key.substr(sep_key + 1) : key;

                // Parse Value: unmangled|symbol
                auto sep_val = value.find('|');
                String unmangled = (sep_val != String::npos) ? value.substr(0, sep_val) : value;
                String sym = (sep_val != String::npos) ? value.substr(sep_val + 1) : "";

                // Check if symbol or unmangled contains query (LIKE %query%)
                if (sym.find(query) != String::npos || unmangled.find(query) != String::npos) {
                    result.push_back(std::format("{}: {}", lib, unmangled));
                }
            }

            unqlite_kv_cursor_release(m_db, cursor);
            return result;
        }

        void export_csv(std::ostream& os) {
            ensure_initialized_for_read();

            // CSV header
            os << "library,symbol,mangled,unmangled\n";

            unqlite_kv_cursor* cursor;
            int rc = unqlite_kv_cursor_init(m_db, &cursor);
            if (rc != unqlite_flags::OK) return;

            for (unqlite_kv_cursor_first_entry(cursor); unqlite_kv_cursor_valid_entry(cursor); unqlite_kv_cursor_next_entry(cursor)) {
                // Fetch Key
                int nKeyLen = 0;
                unqlite_kv_cursor_key(cursor, nullptr, &nKeyLen);
                String key(nKeyLen, 0);
                unqlite_kv_cursor_key(cursor, key.data(), &nKeyLen);

                // Fetch Value
                unqlite_int64 nDataLen = 0;
                unqlite_kv_cursor_data(cursor, nullptr, &nDataLen);
                String value(nDataLen, 0);
                unqlite_kv_cursor_data(cursor, value.data(), &nDataLen);

                // Parse Key: library|mangled
                auto sep_key = key.find('|');
                String lib = (sep_key != String::npos) ? key.substr(0, sep_key) : "unknown";
                String mangled = (sep_key != String::npos) ? key.substr(sep_key + 1) : key;

                // Parse Value: unmangled|symbol
                auto sep_val = value.find('|');
                String unmangled = (sep_val != String::npos) ? value.substr(0, sep_val) : value;
                String symbol = (sep_val != String::npos) ? value.substr(sep_val + 1) : "";

                os << csv_escape(lib) << ',' 
                   << csv_escape(symbol) << ',' 
                   << csv_escape(mangled) << ',' 
                   << csv_escape(unmangled) << '\n';
            }

            unqlite_kv_cursor_release(m_db, cursor);
        }
    };

    export class Storage {
        std::unique_ptr<StorageImpl> m_impl;

    public:
        Storage(String const &db_path = (std::filesystem::temp_directory_path() / "db/libraries_unqlite.db").string())
            : m_impl(std::make_unique<StorageImpl>(db_path)) {
        }

        ~Storage() = default;
        Storage(Storage&&) = default;
        Storage& operator=(Storage&&) = default;

        void insert(Symbol const &symbol) {
            m_impl->insert(symbol);
        }

        Strings find(String const &symbol) {
            return m_impl->find(symbol);
        }

        void export_csv(std::ostream& os) {
            m_impl->export_csv(os);
        }
    };
}  // namespace db