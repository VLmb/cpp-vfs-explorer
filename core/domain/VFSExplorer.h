#pragma once
#include <memory>
#include <utility>
#include <vector>
#include <list>

#include "../search/FileHashMap.h"
#include "../search/FileNameTrie.h"
#include "../utils/PathUtils.h"
#include "VFSDirectory.h"
#include "VFSFile.h"
#include "VFSNode.h"

class VFSExplorer {
  private:
    std::unique_ptr<VFSDirectory> root;
    FileHashMap searchMap;
    FileNameTrie trie;

    VFSDirectory* navigateToDirectory(const std::string& path) const {
        VFSNode* node = navigateToNode(path);
        if (node && node->isDirectory()) {
            return static_cast<VFSDirectory*>(node);
        } else {
            throw std::runtime_error("Directory does not exist at path: " + path);
        }
    }

    VFSFile* navigateToFile(const std::string& path) const {
        VFSNode* node = navigateToNode(path);
        if (node && !node->isDirectory()) {
            return static_cast<VFSFile*>(node);
        } else {
            throw std::runtime_error("File does not exist at path: " + path);
        }
    }

    VFSNode* navigateToNode(const std::string& path) const {
        if (path == "/" || path.empty()) {
            return root.get();
        }

        std::vector<std::string> parts = PathUtils::split(path);
        VFSNode* current = root.get();

        for (size_t i = 0; i < parts.size(); ++i) {
            if (!current->isDirectory()) {
                return nullptr;
            }

            auto* dir = static_cast<VFSDirectory*>(current);
            VFSNode* child = dir->getChild(parts[i]);

            if (!child) {
                return nullptr;
            }

            current = child;
        }

        return current;
    }

    VFSDirectory* getParentDirectory(const std::string& path) const {
        if (path == "/" || path.empty()) {
            return root.get();
        }

        std::vector<std::string> parts = PathUtils::split(path);
        parts.pop_back();
        VFSDirectory* current = root.get();

        for (const auto& part : parts) {
            VFSNode* child = current->getChild(part);
            if (child && child->isDirectory()) {
                current = static_cast<VFSDirectory*>(child);
            } else {
                throw std::runtime_error("Directory does not exist in path: " + path);
            }
        }
        return current;
    }

    void searchRecursive(VFSNode* current, const std::string& targetName,
                         std::vector<VFSNode*>& results) const {
        if (!current)
            return;

        if (current->getName() == targetName) {
            results.push_back(current);
        }

        if (current->isDirectory()) {
            auto* dir = static_cast<VFSDirectory*>(current);
            for (const auto& child : dir->getChildren()) {
                searchRecursive(child.get(), targetName, results);
            }
        }
    }

    void removeFromMap(VFSNode* node) {
        if (!node)
            return;

        searchMap.remove(node->getName(), node);

        if (node->isDirectory()) {
            auto* dir = static_cast<VFSDirectory*>(node);
            for (const auto& child : dir->getChildren()) {
                removeFromMap(child.get());
            }
        }
    }

  public:
    VFSExplorer() : root(std::make_unique<VFSDirectory>("root", nullptr)), searchMap(), trie() {}

    VFSDirectory* getRoot() const { return root.get(); }

    void createDirectory(const std::string& parentPath, const std::string& name) {
        VFSDirectory* parentDir = navigateToDirectory(parentPath);

        if (parentDir->getChild(name)) {
            throw std::runtime_error("Directory or file with the same name already exists");
        }

        auto newDir = std::make_unique<VFSDirectory>(name, parentDir);
        searchMap.put(name, newDir.get());
        trie.insert(name);
        parentDir->add(std::move(newDir));
    }

    void createFile(const std::string& parentPath, const std::string& name,
                    const std::string& physicalPath) {
        VFSDirectory* parentDir = navigateToDirectory(parentPath);

        if (parentDir->getChild(name)) {
            throw std::runtime_error("Directory or file with the same name already exists");
        }

        std::unique_ptr<VFSFile> newFile = std::make_unique<VFSFile>(name, physicalPath, parentDir);
        searchMap.put(name, newFile.get());
        trie.insert(name);
        parentDir->add(std::move(newFile));
    }

    void deleteNode(VFSNode* node) {
        if (!node) {
            throw std::runtime_error("Node is null");
        }

        deleteNode(
            findVirtualPath(node)
        );
    }

    void deleteNode(const std::string& fullPath) {
        VFSDirectory* parentDir = getParentDirectory(fullPath);
        VFSNode* nodeToDelete = parentDir->getChild(PathUtils::split(fullPath).back());
        if (!nodeToDelete) {
            throw std::runtime_error("Node does not exist at path: " + fullPath);
        }
        removeFromMap(nodeToDelete);
        trie.erase(nodeToDelete->getName());
        parentDir->remove(nodeToDelete->getName());
    }

    std::vector<VFSNode*> searchByIndex(const std::string& name) const {
        return searchMap.get(name);
    }

    std::vector<VFSNode*> searchByTraversal(const std::string& name) const {
        std::vector<VFSNode*> results;
        searchRecursive(root.get(), name, results);

        return results;
    }

    bool renameNode(VFSNode* node, const std::string& newName) {
        if (!node) {
            throw std::runtime_error("Node is null");
        }

        renameNode(
            findVirtualPath(node),
            newName
        );

        return true;
    }

    bool renameNode(const std::string& fullPath, const std::string& newName) {
        VFSDirectory* parentDir = getParentDirectory(fullPath);
        VFSNode* nodeToRename = parentDir->getChild(PathUtils::split(fullPath).back());
        if (!nodeToRename) {
            throw std::runtime_error("Node does not exist at path: " + fullPath);
        }

        if (parentDir->getChild(newName)) {
            throw std::runtime_error("A node with the new name already exists in the directory");
        }

        removeFromMap(nodeToRename);
        nodeToRename->rename(newName);
        trie.erase(nodeToRename->getName());
        trie.insert(newName);
        searchMap.put(newName, nodeToRename);

        return true;
    }

    void moveNode(VFSNode* node, VFSDirectory* newParent) {
        if (!node || !newParent) {
            throw std::runtime_error("Node or new parent is null");
        }

        if (!newParent->isDirectory()) {
            throw std::runtime_error("New parent is not a directory");
        }

        if (node->isDirectory()) {
            createDirectory(
                findVirtualPath(newParent),
                node->getName()
            );
        } else {
            auto* fileNode = static_cast<VFSFile*>(node);
            createFile(
                findVirtualPath(newParent),
                node->getName(),
                fileNode->getPhysicalPath()
            );
        }
        deleteNode(node);
    }

    std::string findVirtualPath(VFSNode* node) const {
        if (!node)
            return nullptr;

        if (node == root.get()) {
            return "/";
        }

        std::list<std::string> parts;
        auto current = node;

        while (current && current != root.get()) {
            parts.push_front(current->getName());
            current = current->getParent();
        }

        std::string fullPath;
        for (const auto& part : parts) {
            fullPath += "/" + part;
        }

        return fullPath;
    }

    std::vector<std::string> getSuggestions(const std::string& prefix) const {
        return trie.autoComplete(prefix);
    }
};