#pragma once

#include <map>
#include <memory>
#include <vector>
#include <string>

struct TrieNode {
  std::map<char, std::unique_ptr<TrieNode>> children;
  std::size_t count;   

  explicit TrieNode(std::size_t count = 0) : count(count) {}
};

class Trie {
private:
  std::unique_ptr<TrieNode> root;

  bool search_recursive(TrieNode* node, const std::string& word, std::size_t pos) const {
    if (pos == word.size()) {
      return node->count > 0;
    }

    auto it = node->children.find(word[pos]);
    if (it == node->children.end()) {
      return false;
    }

    return search_recursive(it->second.get(), word, pos + 1);
  }

  void insert_recursive(TrieNode* node, const std::string& word, std::size_t pos) {
    if (pos == word.size()) {
      ++node->count;              
      return;
    }

    char c = word[pos];
    auto it = node->children.find(c);
    if (it == node->children.end()) {
      auto new_node = std::make_unique<TrieNode>();
      TrieNode* child_ptr = new_node.get();
      node->children.emplace(c, std::move(new_node));
      insert_recursive(child_ptr, word, pos + 1);
    } else {
      insert_recursive(it->second.get(), word, pos + 1);
    }
  }

  std::vector<std::string> collect_word(const TrieNode* node,
                                        const std::string& current_word,
                                        std::vector<std::string>& results) const {
    if (node->count > 0) {  
      results.push_back(current_word);
    }

    for (const auto& [ch, child] : node->children) {
      collect_word(child.get(), current_word + ch, results);
    }

    return results;
  }

  bool erase_recursive(TrieNode* node, const std::string& word,
                       std::size_t pos, bool& deleted) {
    if (pos == word.size()) {
      if (node->count == 0) {
        return false;               
      }
      --node->count;                
      deleted = true;

      return node->count == 0 && node->children.empty();
    }

    auto it = node->children.find(word[pos]);
    if (it == node->children.end()) {
      return false;                 
    }

    bool should_delete_child =
        erase_recursive(it->second.get(), word, pos + 1, deleted);

    if (should_delete_child) {
      node->children.erase(it);    
    }

    return node->count == 0 && node->children.empty();
  }

public:
  explicit Trie() : root(std::make_unique<TrieNode>()) {}

  bool search(const std::string& word) const {
    if (word.empty()) return false;
    return search_recursive(root.get(), word, 0);
  }

  void insert(const std::string& word) {
    if (word.empty()) return;
    insert_recursive(root.get(), word, 0);
  }

  std::vector<std::string> auto_complete(const std::string& current_word) const {
    std::vector<std::string> results;
    const TrieNode* node = root.get();

    for (char c : current_word) {
      auto it = node->children.find(c);
      if (it == node->children.end()) {
        return results;
      }
      node = it->second.get();
    }

    collect_word(node, current_word, results);
    return results;
  }

  bool erase(const std::string& word) {
    if (word.empty()) return false;
    bool deleted = false;
    erase_recursive(root.get(), word, 0, deleted);
    return deleted;
  }

};
