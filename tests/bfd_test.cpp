#include <bfd.h>
#include <gtest/gtest.h>
#include <stdio.h>

import std;
import types;
import bfd_wrapper_module;

auto out = &std::cerr;

Strings exec (StrView cmd) {
    std::array<char, 1024>                   buffer;
    std::string                              output;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.data(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    Strings result;
    auto    sstream{ std::stringstream{ output } };

    for (std::string line; std::getline(sstream, line, '\n');) {
        result.push_back(line);
    }

    return result;
}

struct Archive {
    String    m_name;
    StringSet m_members;

    Archive (StrView name) : m_name{ name } {}
};

using Archives = std::forward_list<Archive>;

void dump (StrView title, Archives const &archives) {
    std::print("{}:\n", title);
    for (auto const &archive : archives) {
        std::print("  {}:\n", archive.m_name);
        for (auto const &member : archive.m_members) {
            std::print("    {}\n", member);
        }
    }
}

Archives get_content (StrView filename) {
    std::regex re{ "[0 ]+. (.+)" };
    auto       checked{ exec(std::format("nm -C {}", filename)) };
    Archives   result;

    for (auto const &line : checked) {
        if (line.ends_with(':')) {
            result.emplace_front(line);
        } else {
            std::smatch member;

            if (std::regex_search(line, member, re) && member.size() > 1) {
                if (auto name{ member.str(1) }; !name.starts_with('.')) {
                    assert(!result.empty());
                    auto [_, is_new]{ result.front().m_members.emplace(name) };
                    assert(is_new);
                }
            }
        }
    }

    return result;
}

namespace TestData {
    std::filesystem::path THIS_FILE{ __FILE__ };
    String const          LIB{ (THIS_FILE.parent_path() / "data/libLLVMDWARFLinker.a").string() };
};  // namespace TestData

struct BfdWrapperTestsF : public testing::Test {
    BfdWrapper m_file;

    BfdWrapperTestsF () : m_file{ TestData::LIB } {}
};

TEST_F (BfdWrapperTestsF, NotExistenFile) {
    EXPECT_THROW(BfdWrapper("not a file"), Exception::CouldNotOpen);
}

TEST_F (BfdWrapperTestsF, IsArchive) {
    ASSERT_TRUE(m_file.is_format(bfd_archive));
}

TEST_F (BfdWrapperTestsF, CompareWithNM) {
    auto verified{ get_content(m_file.filename()) };
    dump("verified", verified);

    Archives content;

    for (auto const &bfd : BfdRange(m_file)) {
        auto &archive{ content.emplace_front(bfd.filename()) };
        for (auto const symbol : bfd.symbols()) {
            std::string_view name{ symbol->name };
            if (auto demangled = bfd.demangle(name); !demangled.empty()) {
                archive.m_members.emplace(demangled);
            }
        };
    }
    dump("content", content);
}
