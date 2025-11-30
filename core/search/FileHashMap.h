#pragma once
#include <string>
#include <vector>
#include "model/VFSNode.h"
#include <list>
#include <utility>

struct Entry {
    std::string key;
    std::vector<VFSNode*> values;
};

class FileHashMap {
private:
    static constexpr size_t DEFAULT_CAPACITY = 16;
    static constexpr double LOAD_FACTOR = 0.75;
    static constexpr size_t DJB2_SEED = 5381;
    static constexpr size_t GROWTH_FACTOR = 2;

    std::vector<std::list<Entry>> buckets;
    size_t countOfElements;

    size_t hashFunction(const std::string& str) const {
        size_t hash = DJB2_SEED;
        for (char c : str) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
        }
        return hash;
    }

    size_t getBucketIndex(const std::string& key) const {
        return hashFunction(key) % buckets.size();
    }

    void resize() {
        size_t newCapacity = buckets.size() * GROWTH_FACTOR;
        std::vector<std::list<Entry>> newBuckets(newCapacity);

        for (auto& bucket : buckets) {
            for (auto& entry : bucket) {
                size_t newHash = hashFunction(entry.key);
                size_t newIndex = newHash % newCapacity;
                newBuckets[newIndex].push_back(std::move(entry));
            }
        }
        
        buckets = std::move(newBuckets);
    }

public:
    
    FileHashMap(size_t initialCapacity = DEFAULT_CAPACITY)
    : buckets(initialCapacity == 0 ? 1 : initialCapacity),
      countOfElements(0) {}

    void put(const std::string& key, VFSNode* node) {
        if (countOfElements > buckets.size() * LOAD_FACTOR) {
            resize();
        }

        size_t index = getBucketIndex(key);
        
        for (auto& entry : buckets[index]) {
            if (entry.key == key) {
                entry.values.push_back(node);
                return;
            }
        }

        Entry newEntry;
        newEntry.key = key;
        newEntry.values.push_back(node);
        buckets[index].push_back(std::move(newEntry));
        countOfElements++;
    }

    std::vector<VFSNode*> get(const std::string& key) const {
        size_t index = getBucketIndex(key);

        for (const auto& entry : buckets[index]) {
            if (entry.key == key) {
                return entry.values;
            }
        }
        return {}; 
    }

    void remove(const std::string& key, VFSNode* node) {
        std::size_t index = getBucketIndex(key);
        auto& bucket = buckets[index];

        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if (it->key != key) {
                continue;
            }

            auto& vals = it->values;

            for (auto v = vals.begin(); v != vals.end(); ++v) {
                if (*v == node) {       
                    vals.erase(v);
                    break;
                }
            }

            if (vals.empty()) {
                bucket.erase(it);
                --countOfElements;
            }

            return; 
        }
    }
};