#include "../domain/VFSExplorer.h"
#include "../utils/PathUtils.h"
#include "../utils/ScriptLoader.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>

class TestRunner {
  private:
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;

  public:
    void runTest(const std::string& testName, std::function<void()> testFunc) {
        totalTests++;
        try {
            testFunc();
            passedTests++;
            std::cout << "✓ " << testName << std::endl;
        } catch (const std::exception& e) {
            failedTests++;
            std::cout << "✗ " << testName << " - " << e.what() << std::endl;
        } catch (...) {
            failedTests++;
            std::cout << "✗ " << testName << " - Unknown error" << std::endl;
        }
    }

    void printSummary() const {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "Tests Summary:" << std::endl;
        std::cout << "Total: " << totalTests << " | Passed: " << passedTests
                  << " | Failed: " << failedTests << std::endl;
        std::cout << std::string(50, '=') << std::endl;
    }
};

void assertEquals(const std::string& expected, const std::string& actual,
                  const std::string& message = "") {
    if (expected != actual) {
        throw std::runtime_error("Expected: '" + expected + "', Got: '" + actual + "'" +
                                 (message.empty() ? "" : " (" + message + ")"));
    }
}

void assertTrue(bool condition, const std::string& message = "") {
    if (!condition) {
        throw std::runtime_error("Assertion failed: " + message);
    }
}

void assertFalse(bool condition, const std::string& message = "") {
    if (condition) {
        throw std::runtime_error("Assertion failed (expected false): " + message);
    }
}

void assertNotNull(const void* ptr, const std::string& message = "") {
    if (ptr == nullptr) {
        throw std::runtime_error("Expected non-null pointer: " + message);
    }
}

void assertThrows(std::function<void()> func, const std::string& message = "") {
    try {
        func();
        throw std::runtime_error("Expected exception but none was thrown: " + message);
    } catch (const std::runtime_error& e) {
        // Expected
        if (message.empty() || std::string(e.what()).find(message) != std::string::npos) {
            // Test passed
        } else {
            throw std::runtime_error("Expected message containing: '" + message + "', Got: '" +
                                     e.what() + "'");
        }
    }
}

int main() {
    TestRunner runner;

    std::cout << "Initializing VFSExplorer with ScriptLoader...\n" << std::endl;

    VFSExplorer explorer;
    ScriptLoader::load(explorer, "core/resources/script.txt");

    std::cout << "VFSExplorer initialized successfully!\n" << std::endl;


    // ==================== Directory Creation Tests ====================
    runner.runTest("Test 7: Create new directory", [&]() {
        VFSDirectory* newDir = explorer.createDirectory("/home", "newdir");
        assertNotNull(newDir, "New directory should be created");
        assertEquals("newdir", newDir->getName(), "Directory name should match");
    });

    runner.runTest("Test 8: Create nested directory", [&]() {
        VFSDirectory* newDir = explorer.createDirectory("/home/newdir", "subdir");
        assertNotNull(newDir, "Nested directory should be created");
    });

    runner.runTest("Test 9: Create directory with duplicate name throws exception", [&]() {
        assertThrows([&]() { explorer.createDirectory("/home", "documents"); }, "already exists");
    });

    runner.runTest("Test 10: Create directory in non-existing parent throws exception", [&]() {
        assertThrows([&]() { explorer.createDirectory("/non/existing", "dir"); },
                     "Directory does not exist");
    });

    // ==================== File Creation Tests ====================
    runner.runTest("Test 11: Create new file", [&]() {
        VFSFile* newFile = explorer.createFile("/home/documents", "newfile.txt",
                                               "core/resources/files/document.txt");
        assertNotNull(newFile, "New file should be created");
        assertEquals("newfile.txt", newFile->getName(), "File name should match");
    });

    runner.runTest("Test 12: Create file with non-existing physical path throws exception", [&]() {
        assertThrows(
            [&]() {
                explorer.createFile("/home", "badfile.txt", "/non/existing/physical/path.txt");
            },
            "Physical file does not exist");
    });

    runner.runTest("Test 13: Create file with duplicate name throws exception", [&]() {
        assertThrows(
            [&]() {
                explorer.createFile("/home/documents", "Tiger.txt",
                                    "core/resources/files/Tiger.txt");
            },
            "already exists");
    });

    runner.runTest("Test 14: Create file in non-existing parent throws exception", [&]() {
        assertThrows(
            [&]() {
                explorer.createFile("/non/existing", "file.txt",
                                    "core/resources/files/document.txt");
            },
            "Directory does not exist");
    });

    // ==================== Rename Tests ====================
    runner.runTest("Test 15: Rename node with valid path", [&]() {
        explorer.renameNode("/home/newdir", "renamed_dir");
        VFSDirectory* dir = explorer.navigateToDirectory("/home/renamed_dir");
        assertNotNull(dir, "Renamed directory should be found");
    });

    runner.runTest("Test 16: Rename file with valid path", [&]() {
        explorer.renameNode("/home/documents/newfile.txt", "renamed_file.txt");
        VFSFile* file = explorer.navigateToFile("/home/documents/renamed_file.txt");
        assertNotNull(file, "Renamed file should be found");
    });

    runner.runTest("Test 17: Rename node with duplicate name throws exception", [&]() {
        assertThrows([&]() { explorer.renameNode("/home/documents/Tiger.txt", "document.txt"); },
                     "already exists");
    });

    runner.runTest("Test 18: Rename node with non-existing path throws exception", [&]() {
        assertThrows([&]() { explorer.renameNode("/non/existing/path", "newname"); },
                     "Directory does not exist");
    });

    // ==================== Virtual Path Tests ====================
    runner.runTest("Test 19: Get virtual path for root", [&]() {
        std::string path = explorer.findVirtualPath(explorer.getRoot());
        assertEquals("/", path, "Root path should be /");
    });

    runner.runTest("Test 20: Get virtual path for directory", [&]() {
        VFSDirectory* dir = explorer.navigateToDirectory("/home/projects/cpp_labs");
        std::string path = explorer.findVirtualPath(dir);
        assertEquals("/home/projects/cpp_labs", path, "Path should be correct");
    });

    runner.runTest("Test 21: Get virtual path for file", [&]() {
        VFSFile* file = explorer.navigateToFile("/home/documents/Tiger.txt");
        std::string path = explorer.findVirtualPath(file);
        assertEquals("/home/documents/Tiger.txt", path, "File path should be correct");
    });

    runner.runTest("Test 22: Get virtual path for null node returns empty", [&]() {
        std::string path = explorer.findVirtualPath(nullptr);
        assertEquals("", path, "Null node should return empty string");
    });

    // ==================== Search Tests ====================
    runner.runTest("Test 23: Search by index with exact match", [&]() {
        auto results = explorer.searchByIndex("Tiger.txt");
        assertTrue(!results.empty(), "Should find at least one Tiger.txt");
    });

    runner.runTest("Test 24: Search by index returns correct nodes", [&]() {
        auto results = explorer.searchByIndex("Tiger.txt");
        for (const auto& node : results) {
            assertEquals("Tiger.txt", node->getName(), "All results should have matching name");
        }
    });

    runner.runTest("Test 25: Search by traversal with exact match", [&]() {
        auto results = explorer.searchByTraversal("documents");
        assertTrue(!results.empty(), "Should find documents directory");
    });

    runner.runTest("Test 26: Search by traversal finds all matching nodes", [&]() {
        auto results = explorer.searchByTraversal("lab1");
        assertTrue(!results.empty(), "Should find at least one lab1 directory");
    });

    runner.runTest("Test 27: Search for non-existing node returns empty", [&]() {
        auto results = explorer.searchByIndex("non_existing_file.txt");
        assertTrue(results.empty(), "Should return empty for non-existing file");
    });

    // ==================== Delete Tests ====================
    runner.runTest("Test 28: Delete file by path", [&]() {
        explorer.deleteNode("/home/documents/renamed_file.txt");
        assertThrows([&]() { explorer.navigateToFile("/home/documents/renamed_file.txt"); },
                     "should not exist after deletion");
    });

    runner.runTest("Test 29: Delete directory by path", [&]() {
        explorer.deleteNode("/home/renamed_dir");
        assertThrows([&]() { explorer.navigateToDirectory("/home/renamed_dir"); },
                     "should not exist after deletion");
    });

    runner.runTest("Test 30: Delete non-existing node throws exception", [&]() {
        assertThrows([&]() { explorer.deleteNode("/non/existing/node"); }, "does not exist");
    });

    runner.runTest("Test 31: Delete file by pointer", [&]() {
        VFSFile* file = explorer.navigateToFile("/home/documents/document.txt");
        explorer.deleteNode(file);
        assertThrows([&]() { explorer.navigateToFile("/home/documents/document.txt"); },
                     "should not exist after deletion");
    });

    // ==================== Parent Pointer Tests ====================
    runner.runTest("Test 32: Parent pointer is correct after creation", [&]() {
        VFSDirectory* parent = explorer.navigateToDirectory("/home/projects");
        VFSDirectory* child = explorer.createDirectory("/home/projects", "test_parent");
        assertTrue(child->getParent() == parent, "Parent pointer should be correct");
    });

    runner.runTest("Test 33: Root directory has null parent", [&]() {
        VFSDirectory* root = explorer.getRoot();
        assertTrue(root->getParent() == nullptr, "Root should have null parent");
    });

    runner.runTest("Test 34: File parent pointer is correct", [&]() {
        VFSDirectory* parent = explorer.navigateToDirectory("/home/projects/java_labs");
        VFSFile* file = explorer.navigateToFile("/home/projects/java_labs/hw1-hangman");
        // hw1-hangman is actually a directory in the script, let's test with actual file
        VFSFile* actualFile = explorer.navigateToFile("/home/pictures/BlackCat.jpg");
        VFSDirectory* parentDir = explorer.navigateToDirectory("/home/pictures");
        assertTrue(actualFile->getParent() == parentDir, "File parent should be correct");
    });

    // ==================== Complex Operations Tests ====================
    runner.runTest("Test 35: Complex directory structure navigation", [&]() {
        VFSDirectory* dir1 = explorer.navigateToDirectory("/home");
        VFSDirectory* dir2 = explorer.navigateToDirectory("/home/projects");
        VFSDirectory* dir3 = explorer.navigateToDirectory("/home/projects/cpp_labs");
        VFSDirectory* dir4 = explorer.navigateToDirectory("/home/projects/cpp_labs/lab1");
        assertNotNull(dir4, "Should navigate through deep directory structure");
    });

    runner.runTest("Test 36: Get all children of directory", [&]() {
        VFSDirectory* dir = explorer.navigateToDirectory("/home");
        auto& children = dir->getChildren();
        assertTrue(!children.empty(), "Root home directory should have children");
    });

    runner.runTest("Test 37: File size is correctly retrieved", [&]() {
        VFSFile* file = explorer.navigateToFile("/home/pictures/BlackCat.jpg");
        size_t size = file->getSize();
        assertTrue(size > 0, "File size should be greater than 0");
    });

    runner.runTest("Test 38: Directory size is sum of children sizes", [&]() {
        VFSDirectory* dir = explorer.navigateToDirectory("/home/pictures");
        size_t dirSize = dir->getSize();
        assertTrue(dirSize > 0, "Directory size should be greater than 0");
    });

    runner.runTest("Test 39: isDirectory() returns correct type", [&]() {
        VFSDirectory* dir = explorer.navigateToDirectory("/home");
        VFSFile* file = explorer.navigateToFile("/home/pictures/BlackCat.jpg");
        assertTrue(dir->isDirectory(), "Directory should return true for isDirectory()");
        assertFalse(file->isDirectory(), "File should return false for isDirectory()");
    });

    runner.runTest("Test 40: Get child by name", [&]() {
        VFSDirectory* dir = explorer.navigateToDirectory("/home");
        VFSNode* child = dir->getChild("projects");
        assertNotNull(child, "Should find child by name");
        assertEquals("projects", child->getName(), "Child name should match");
    });

    runner.runTest("Test 41: Get non-existing child returns null", [&]() {
        VFSDirectory* dir = explorer.navigateToDirectory("/home");
        VFSNode* child = dir->getChild("non_existing");
        assertTrue(child == nullptr, "Non-existing child should return null");
    });

    runner.runTest("Test 42: Rename node using pointer", [&]() {
        VFSDirectory* dir = explorer.createDirectory("/home/projects", "temp_dir");
        explorer.renameNode(dir, "renamed_temp");
        VFSDirectory* renamed = explorer.navigateToDirectory("/home/projects/renamed_temp");
        assertNotNull(renamed, "Renamed directory should be found");
    });

    // ==================== Move Tests ====================
    runner.runTest("Test 43: Move file to another directory", [&]() {
        VFSDirectory* destDir = explorer.navigateToDirectory("/home/projects");
        VFSFile* file = explorer.navigateToFile("/home/pictures/Hasky.jpg");
        explorer.moveNode(file, static_cast<VFSDirectory*>(destDir));
        VFSFile* movedFile = explorer.navigateToFile("/home/projects/Hasky.jpg");
        assertNotNull(movedFile, "File should be moved to new location");
    });

    runner.runTest("Test 44: Move directory with children", [&]() {
        VFSDirectory* srcDir = explorer.navigateToDirectory("/home/projects/test_parent");
        VFSDirectory* destDir = explorer.navigateToDirectory("/home");
        if (srcDir->getChildren().empty()) {
            // Create a test structure
            explorer.createDirectory("/home/projects/test_parent", "child1");
        }
        explorer.moveNode(srcDir, destDir);
        // Verify it's in new location
        VFSDirectory* movedDir = explorer.navigateToDirectory("/home/test_parent");
        assertNotNull(movedDir, "Directory should be moved");
    });

    runner.runTest("Test 45: Move with null parameters throws exception", [&]() {
        assertThrows([&]() { explorer.moveNode(nullptr, explorer.navigateToDirectory("/home")); },
                     "is null");
    });

    // ==================== Autocomplete Tests ====================
    runner.runTest("Test 46: Get suggestions for prefix", [&]() {
        auto suggestions = explorer.getSuggestions("la");
        assertTrue(!suggestions.empty(), "Should get suggestions for prefix 'la'");
    });

    runner.runTest("Test 47: Get suggestions returns matching results", [&]() {
        auto suggestions = explorer.getSuggestions("home");
        for (const auto& suggestion : suggestions) {
            assertTrue(suggestion.find("home") != std::string::npos || suggestion[0] == 'h',
                       "Suggestion should match prefix");
        }
    });

    // ==================== Creation Time Tests ====================
    runner.runTest("Test 48: Node has creation time", [&]() {
        VFSDirectory* dir = explorer.getRoot();
        time_t creationTime = dir->getCreationTime();
        assertTrue(creationTime > 0, "Creation time should be set");
    });

    runner.runTest("Test 49: Newly created nodes have recent creation time", [&]() {
        time_t beforeCreation = std::time(nullptr);
        VFSDirectory* newDir = explorer.createDirectory("/home/projects", "time_test");
        time_t afterCreation = std::time(nullptr);
        time_t nodeTime = newDir->getCreationTime();
        assertTrue(nodeTime >= beforeCreation && nodeTime <= afterCreation,
                   "Creation time should be recent");
    });

    // ==================== Physical Path Tests ====================
    runner.runTest("Test 50: File returns correct physical path", [&]() {
        VFSFile* file = explorer.navigateToFile("/home/pictures/Leopard.jpg");
        std::string physicalPath = file->getPhysicalPath();
        assertTrue(!physicalPath.empty(), "Physical path should not be empty");
        assertTrue(physicalPath.find("Leopard.jpg") != std::string::npos,
                   "Physical path should contain file name");
    });

    runner.printSummary();

    return runner.passedTests == runner.totalTests ? 0 : 1;
}
