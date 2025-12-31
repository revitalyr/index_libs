#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

import std;
import types;
import storage_module;
import indexer_module;

namespace fs = std::filesystem;

namespace TestData {
    fs::path const THIS_FILE{ __FILE__ };
    fs::path const DATA_DIR{ THIS_FILE.parent_path() / "data" };
    
#ifdef _WIN32
    String const LIB_FILE{ (DATA_DIR / "index_libs.lib").string() };
#else
    String const LIB_FILE{ (DATA_DIR / "libLLVMDWARFLinker.a").string() };
#endif
}

class StorageTestFixture : public testing::Test {
protected:
    fs::path m_db_path;
    
    void SetUp() override {
        m_db_path = fs::temp_directory_path() / 
            std::format("test_db_{}.db", std::chrono::steady_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override {
        if (fs::exists(m_db_path)) {
            fs::remove(m_db_path);
        }
    }
};

// =============================================================================
// Storage Tests
// =============================================================================

TEST_F(StorageTestFixture, InsertAndFind) {
    db::Storage storage(m_db_path.string());
    storage.insert(db::Symbol{ "test.obj", "TestFunction", "_Z12TestFunctionv", "TestFunction()" });
    auto results = storage.find("TestFunction");
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results[0].find("TestFunction") != std::string::npos);
}

TEST_F(StorageTestFixture, FindNonExistent) {
    db::Storage storage(m_db_path.string());
    storage.insert(db::Symbol{ "test.obj", "Foo", "_Z3Foov", "Foo()" });
    auto results = storage.find("NonExistentSymbol");
    EXPECT_TRUE(results.empty());
}

TEST_F(StorageTestFixture, FindSubstring) {
    db::Storage storage(m_db_path.string());
    storage.insert(db::Symbol{ "test.obj", "MyNamespace::MyClass::MyMethod", 
                               "_ZN11MyNamespace7MyClass8MyMethodEv", 
                               "MyNamespace::MyClass::MyMethod()" });
    auto results = storage.find("MyClass");
    ASSERT_EQ(results.size(), 1);
}

TEST_F(StorageTestFixture, DeduplicateResults) {
    db::Storage storage(m_db_path.string());
    storage.insert(db::Symbol{ "test.obj", "Duplicate", "_Z9Duplicatev", "Duplicate()" });
    storage.insert(db::Symbol{ "test.obj", "Duplicate", "_Z9Duplicatei", "Duplicate()" });
    auto results = storage.find("Duplicate");
    EXPECT_EQ(results.size(), 1);
}

TEST_F(StorageTestFixture, ExportCSV) {
    db::Storage storage(m_db_path.string());
    storage.insert(db::Symbol{ "lib.obj", "func", "_Z4funcv", "func()" });
    std::ostringstream oss;
    storage.export_csv(oss);
    std::string csv = oss.str();
    EXPECT_TRUE(csv.find("library,symbol,mangled,unmangled") != std::string::npos);
    EXPECT_TRUE(csv.find("lib.obj") != std::string::npos);
}

// =============================================================================
// Indexer Tests - Platform Specific
// =============================================================================

class IndexerTestFixture : public StorageTestFixture {
protected:
    void SetUp() override {
        StorageTestFixture::SetUp();
        ASSERT_TRUE(fs::exists(TestData::LIB_FILE)) 
            << "Test library not found: " << TestData::LIB_FILE 
            << "\nPlease ensure the test data file exists.";
    }
};

TEST_F(IndexerTestFixture, BuildIndexFromLibrary) {
    db::Storage storage(m_db_path.string());
    EXPECT_NO_THROW(indexer::build_index(TestData::LIB_FILE, storage, false));
}

TEST_F(IndexerTestFixture, BuildIndexVerbose) {
    db::Storage storage(m_db_path.string());
    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(indexer::build_index(TestData::LIB_FILE, storage, true));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
}

#ifdef _WIN32
// =============================================================================
// Windows-specific tests for index_libs.lib (import library)
// =============================================================================

TEST_F(IndexerTestFixture, WindowsLibIndexesSymbols) {
    db::Storage storage(m_db_path.string());
    indexer::build_index(TestData::LIB_FILE, storage, false);
    
    std::ostringstream oss;
    storage.export_csv(oss);
    std::string csv = oss.str();
    
    auto lines = std::count(csv.begin(), csv.end(), '\n');
    EXPECT_GT(lines, 1) << "Expected at least one symbol. CSV:\n" << csv;
}

TEST_F(IndexerTestFixture, WindowsLibFindsDescriptor) {
    db::Storage storage(m_db_path.string());
    indexer::build_index(TestData::LIB_FILE, storage, false);
    
    auto results = storage.find("DESCRIPTOR");
    EXPECT_FALSE(results.empty()) << "Expected to find 'DESCRIPTOR' symbol";
}

TEST_F(IndexerTestFixture, WindowsLibFindsThunkData) {
    db::Storage storage(m_db_path.string());
    indexer::build_index(TestData::LIB_FILE, storage, false);
    
    auto results = storage.find("THUNK_DATA");
    EXPECT_FALSE(results.empty()) << "Expected to find 'THUNK_DATA' symbol";
}

#else
// =============================================================================
// Linux-specific tests for libLLVMDWARFLinker.a
// =============================================================================

TEST_F(IndexerTestFixture, LinuxLibContainsExpectedSymbols) {
    db::Storage storage(m_db_path.string());
    indexer::build_index(TestData::LIB_FILE, storage, false);
    
    // Search for "dwarf_linker" - the actual namespace in LLVM symbols
    auto dwarf_results = storage.find("dwarf_linker");
    EXPECT_FALSE(dwarf_results.empty()) << "Expected to find 'dwarf_linker' symbols";
}

TEST_F(IndexerTestFixture, LinuxLibFindsParseDebugTableName) {
    db::Storage storage(m_db_path.string());
    indexer::build_index(TestData::LIB_FILE, storage, false);
    
    // This function exists in the library: llvm::dwarf_linker::parseDebugTableName
    auto results = storage.find("parseDebugTableName");
    EXPECT_FALSE(results.empty()) << "Expected to find 'parseDebugTableName' symbol";
}

TEST_F(IndexerTestFixture, LinuxLibExportsSymbols) {
    db::Storage storage(m_db_path.string());
    indexer::build_index(TestData::LIB_FILE, storage, false);
    
    // Verify that at least some symbols were indexed
    std::ostringstream oss;
    storage.export_csv(oss);
    std::string csv = oss.str();
    
    auto lines = std::count(csv.begin(), csv.end(), '\n');
    EXPECT_GT(lines, 1) << "Expected at least one symbol. CSV:\n" << csv.substr(0, 500);
}

#endif

// =============================================================================
// Cross-platform Tests
// =============================================================================

TEST(IndexerTest, NonExistentFileThrows) {
    fs::path temp_db = fs::temp_directory_path() / "nonexistent_test.db";
    db::Storage storage(temp_db.string());
    EXPECT_THROW(
        indexer::build_index("/nonexistent/path/to/library.lib", storage, false),
        std::exception
    );
    fs::remove(temp_db);
}

TEST(IndexerTest, LibExtensionIsCorrect) {
#ifdef _WIN32
    EXPECT_EQ(indexer::lib_extension, ".lib");
#else
    EXPECT_EQ(indexer::lib_extension, ".a");
#endif
}

#ifdef _WIN32
TEST(DemangleTest, MSVCMangledName) {
    std::string mangled = "?MyFunction@@YAHXZ";
    std::string demangled = indexer::demangle(mangled);
    EXPECT_FALSE(demangled.empty());
}

TEST(DemangleTest, ItaniumMangledName) {
    std::string mangled = "_ZN5llvm12DWARFLinkerC1Ev";
    std::string demangled = indexer::demangle(mangled);
    EXPECT_TRUE(demangled.find("llvm") != std::string::npos || demangled == mangled);
}

TEST(DemangleTest, UnmangledNameUnchanged) {
    std::string plain = "printf";
    std::string result = indexer::demangle(plain);
    EXPECT_EQ(result, plain);
}
#endif

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}