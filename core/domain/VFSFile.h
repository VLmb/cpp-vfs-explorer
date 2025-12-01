#pragma once
#include "VFSNode.h"
#include <filesystem>
#include <fstream>
#include <memory>

class VFSFile : public VFSNode {
  public:
    VFSFile(std::string name, std::string physicalPath, VFSNode* parent = nullptr)
        : VFSNode(std::move(name), parent) {
        if (!std::filesystem::exists(physicalPath)) {
            throw std::runtime_error("Physical file does not exist: " + physicalPath);
        }
        this->physicalPath = std::filesystem::absolute(physicalPath).string();
    }

    bool isDirectory() const override { return false; }

    size_t getSize() const override {
        std::error_code ec;
        auto size = std::filesystem::file_size(physicalPath, ec);
        return ec ? 0 : size;
    }

    std::string getPhysicalPath() const { 
        return physicalPath;
     }

    std::unique_ptr<std::istream> openReadStream() const {
        return std::make_unique<std::ifstream>(physicalPath, std::ios::binary);
    }

  private:
    std::string physicalPath;
};