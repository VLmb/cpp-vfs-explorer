#pragma once

#include "../domain/VFSExplorer.h"
#include <iostream>

class ScriptLoader {
private:

    static constexpr const char* PATH_TO_SCRIPT = "core/resources/script.txt";
    static constexpr const char* MKFILE_CMD = "mkfile";
    static constexpr const char* MKDIR_CMD = "mkdir";

    static void processMkdir(VFSExplorer& exp, const std::string& fullPath) {
        std::string parent = PathUtils::getParentPath(fullPath);
        std::string name = PathUtils::getFileName(fullPath);
        
        try {
            exp.createDirectory(parent, name);
            std::cout << "Directory created: " << fullPath << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create directory " << fullPath << ": " << e.what() << std::endl;
        }
    }

    static void processMkfile(VFSExplorer& exp, const std::string& vPath, const std::string& rPath) {
        std::string parent = PathUtils::getParentPath(vPath);
        std::string name = PathUtils::getFileName(vPath);
        try {
            exp.addFile(parent, name, rPath);
            std::cout << "File created: " << vPath << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create file " << vPath << ": " << e.what() << std::endl;
        }
    }
public:

    static void load(VFSExplorer& explorer, const char* scriptPath = PATH_TO_SCRIPT) {
        std::ifstream file(scriptPath);
        if (!file.is_open()) {
            std::cerr << "[Error] ScriptLoader: annot open file " << scriptPath << std::endl;
            return;
        }

        std::string line;
        int lineNumber = 0;
        while (std::getline(file, line)) {
            lineNumber++;
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::stringstream ss(line);
            std::string command;
            ss >> command;

            if (command == MKDIR_CMD) {
                std::string fullPath;
                ss >> fullPath;
                if (!fullPath.empty()) {
                    processMkdir(explorer, fullPath);
                }
            } 
            else if (command == MKFILE_CMD) {
                std::string vPath, rPath;
                ss >> vPath >> rPath;
                if (!vPath.empty() && !rPath.empty()) {
                    processMkfile(explorer, vPath, rPath);
                }
            }
            else {
                std::cerr << "[Warning] Unknown command at line " << lineNumber << ": " << command << std::endl;
            }
        }
        std::cout << "[Info] Script loaded successfully." << std::endl;
    }
};
