#pragma once
#include <memory>
#include "Trie.h"

class FileNameTrie {
private:
    std::unique_ptr<Trie> trie;

public:
    FileNameTrie() : trie(std::make_unique<Trie>()) {}

    void insert(const std::string& fileName) {
        trie->insert(fileName);
    }

    bool search(const std::string& fileName) const {
        return trie->search(fileName);
    }

    std::vector<std::string> autoComplete(const std::string& prefix) const {
        return trie->auto_complete(prefix);
    }

    bool erase(const std::string& fileName) {
        return trie->erase(fileName);
    }

};