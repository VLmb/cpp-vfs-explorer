#pragma once
#include "../domain/VFSExplorer.h"

struct BenchmarkResult {
        long long slowTimeUs;
        long long fastTimeUs;
        int itemsFound;
        int datasetSize;
    };

class BenchmarkService {
public:
    static void fillDirectory(int fileCount, VFSDirectory* dir, VFSExplorer& explorer) {
        for (int i = 0; i < fileCount; ++i) {
            std::string fileName = "file_" + std::to_string(i) + ".txt";
            explorer.addFile( 
                explorer.findVirtualPath(dir),
                fileName,
                "C:\\Fake\\Path.txt" 
            );
        }
    }

    static void generateDataset(VFSExplorer& explorer, int fileCount, int depth = 5) {
        std::srand(std::time(nullptr));
        
        std::string benchRoot = "/benchmark_data";
        explorer.createDirectory("/", "benchmark_data");

        std::vector<std::string> folders;
        folders.push_back(benchRoot);

        for (int d = 0; d < depth; ++d) {

        for (int i = 0; i < fileCount / 10; ++i) {
            std::string parent = folders[std::rand() % folders.size()];
            std::string newDirName = "dir_" + std::to_string(i);
            
            if (explorer.createDirectory(parent, newDirName)) {
                // Строим путь вручную (ленивый способ, лучше бы через PathUtils, но для теста сойдет)
                // ВАЖНО: Тут надо аккуратно с путями, но для генератора сойдет
                std::string newPath = (parent == "/" ? "" : parent) + "/" + newDirName;
                folders.push_back(newPath);
            }
        }

        // 2. Генерируем файлы
        for (int i = 0; i < fileCount; ++i) {
            std::string parent = folders[std::rand() % folders.size()];
            
            // Делаем имена повторяющимися, чтобы поиск возвращал несколько результатов
            // Например: report_0, report_1 ... report_9, снова report_0...
            std::string fileName = "file_" + std::to_string(i % 100) + ".txt"; 
            
            // Монтируем "фейковый" путь (нам не важно, есть ли он на диске для теста поиска)
            explorer.mountFile(parent, fileName, "C:\\Fake\\Path.txt");
        }
        
        std::cout << "[Benchmark] Dataset generated: " << fileCount << " files in " << folders.size() << " folders.\n";
    }

    // Запуск сравнения
    static BenchmarkResult run(core::service::VFSManager& explorer, const std::string& query, int iterations = 100) {
        BenchmarkResult res;
        res.datasetSize = 0; // Можно заполнить, если есть метод countNodes()

        // 1. Тест медленного поиска (Дерево)
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            auto v = explorer.searchSlow(query);
            if (i == 0) res.itemsFound = v.size(); // Запоминаем кол-во найденных
        }
        auto end = std::chrono::high_resolution_clock::now();
        res.slowTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / iterations;

        // 2. Тест быстрого поиска (Хэш-таблица)
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            volatile auto v = explorer.searchFast(query); // volatile чтобы компилятор не выкинул цикл
        }
        end = std::chrono::high_resolution_clock::now();
        res.fastTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / iterations;

        return res;
    }
};