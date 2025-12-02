#pragma once
#include "../domain/VFSExplorer.h"
#include <chrono>
#include <fstream>
#include <iostream>

struct BenchmarkResult {
    long long searchByTraversalTime;
    long long searchByIndexTime;
};

class BenchmarkService {
  private:
    static inline const std::string PHYSICAL_TMP_DIR =
        (std::filesystem::temp_directory_path() / "benchmark_temp_file.txt").string();
    static constexpr const char* VIRTUAL_FILE_PREFIX = "file_";
    static constexpr const char* VIRTUAL_ROOT_DIR = "benchmark_data";
    static constexpr const char* VIRTUAL_DIR_PREFIX = "dir_";

    static void createTempFile() {
        std::ofstream file(PHYSICAL_TMP_DIR);
        if (file.is_open()) {
            file << "Benchmark temporary file\n";
            file.close();
        }
    }

    static void removeTempFile() {
        try {
            if (std::filesystem::exists(PHYSICAL_TMP_DIR)) {
                std::filesystem::remove(PHYSICAL_TMP_DIR);
                std::cout << "Temporary file removed: " << PHYSICAL_TMP_DIR << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to remove temporary file: " << e.what() << std::endl;
        }
    }

  public:
    static void generateDataset(VFSExplorer& explorer, int fileCount) {
        std::srand(std::time(nullptr));

        explorer.createDirectory("/", VIRTUAL_ROOT_DIR);

        std::vector<std::string> directories;
        directories.push_back("/" + std::string(VIRTUAL_ROOT_DIR));

        for (int i = 0; i < fileCount / 5; ++i) {
            size_t randIndex = std::rand() % directories.size();
            std::string parentDir = directories[randIndex];
            std::string newDirName = VIRTUAL_DIR_PREFIX + std::to_string(i);

            VFSDirectory* newDir = explorer.createDirectory(parentDir, newDirName);
            if (newDir != nullptr) {
                std::string newDirPath = parentDir + "/" + newDirName;
                directories.push_back(newDirPath);
            }
        }

        for (int i = 0; i < fileCount; ++i) {
            size_t randIndex = std::rand() % directories.size();
            std::string parentPath = directories[randIndex];
            std::string fileName = VIRTUAL_FILE_PREFIX + std::to_string(i);

            try {
                explorer.createFile(parentPath, fileName, PHYSICAL_TMP_DIR);
            } catch (const std::exception& e) {
                std::cerr << "Failed to create file " << fileName << ": " << e.what() << std::endl;
            }
        }
    }

    static BenchmarkResult run(VFSExplorer& explorer, int filecount = 1000, int iterations = 100) {
        BenchmarkResult result;

        createTempFile();

        generateDataset(explorer, filecount);

        std::vector<std::string> fileNames;
        fileNames.reserve(iterations);
        for (int i = 0; i < iterations; ++i) {
            size_t randIndex = std::rand() % filecount;
            fileNames.push_back(VIRTUAL_FILE_PREFIX + std::to_string(randIndex));
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto v = explorer.searchByTraversal(fileNames[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        result.searchByTraversalTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / iterations;

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto v = explorer.searchByIndex(fileNames[i]);
        }
        end = std::chrono::high_resolution_clock::now();
        result.searchByIndexTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / iterations;

        removeTempFile();

        return result;
    }
};
