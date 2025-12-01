#pragma once
#include "VFSNode.h"
#include <algorithm>
#include <memory>
#include <vector>

class VFSDirectory : public VFSNode {
  public:
    VFSDirectory(std::string name)
        : VFSNode(std::move(name)), children() {}

    bool isDirectory() const override { return true; }

    size_t getSize() const override {
        size_t total = 0;
        for (const auto& child : children) {
            total += child->getSize();
        }
        return total;
    }

    void add(std::unique_ptr<VFSNode> node) {
        if (node) {
            children.push_back(std::move(node));
        }
    }

    bool remove(const std::string& name) {
        for (auto it = children.begin(); it != children.end(); ++it) {
            if ((*it)->getName() == name) {
                children.erase(it);
                return true;
            }
        }
        return false;
    }

    VFSNode* getChild(const std::string& name) const {
        for (const auto& child : children) {
            if (child->getName() == name) {
                return child.get();
            }
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<VFSNode>>& getChildren() const { return children; }

  private:
    std::vector<std::unique_ptr<VFSNode>> children;
};