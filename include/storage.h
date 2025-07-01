//storage.h
#pragma once

import std;

namespace db {
    struct Symbol{
        std::string m_library;
        std::string m_mangled;
        std::string m_unmangled;
        std::string m_symbol;
    public:
        Symbol() = default;
        Symbol(std::string const &library, std::string const &mangled, std::string const &unmangled);

        friend std::basic_ostream<char> &operator<<(std::basic_ostream<char> &ostr, Symbol const &symbol);
    };

	class Storage {
	public:
        using   Strings = const std::vector<std::string>;

		        Storage();
		        ~Storage();

        void    insert(Symbol const &symbol);
        Strings find(std::string const &symbol);
	};
}
