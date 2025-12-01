#pragma once
#include <ctime>
#include <string>

class VFSNode {
  public:
    virtual ~VFSNode() = default;

    VFSNode(std::string name)
        : name(std::move(name)), createdAt(std::time(nullptr)) {}

    std::string getName() const { return name; }

    std::time_t getCreationTime() const { return createdAt; }

    void rename(const std::string& newName) { name = newName; }

    virtual bool isDirectory() const = 0;
    virtual size_t getSize() const = 0;

  protected:
    std::string name;
    std::time_t createdAt;
};
