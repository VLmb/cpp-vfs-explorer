#pragma once
#include "VFSNode.h"
#include <algorithm>
#include <memory>
#include <vector>

class VFSDirectory : public VFSNode {

private:
    std::vector<std::unique_ptr<VFSNode>> children;

public:
    VFSDirectory(std::string name, VFSNode* parent = nullptr)
        : VFSNode(std::move(name), parent), children() {}

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
            node->setParent(this);
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

    const std::vector<std::unique_ptr<VFSNode>>& getChildren() const { 
        return children;
    }

    std::unique_ptr<VFSNode> extractChild(const std::string& name) {
        for (auto it = children.begin(); it != children.end(); ++it) {
            if ((*it)->getName() == name) {
                std::unique_ptr<VFSNode> extractedNode = std::move(*it);
                children.erase(it);
                return extractedNode;
            }
        }
        return nullptr;
    }

    std::unique_ptr<VFSNode> clone() const override {
        auto newDir = std::make_unique<VFSDirectory>(this->getName());
        for (const auto& child : children) {
            newDir->add(child->clone());
        }

        return newDir;
    }
};