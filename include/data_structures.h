#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <chrono>
#include <mutex>
#include <optional>
#include <cstdlib>

// ---------------------------------------------------------
// CORE NODE STRUCTURE
// ---------------------------------------------------------
struct CacheNode {
    std::string key;
    std::string value;
    long long expire_at; // 0 means no expiration

    // LRU Doubly Linked List Pointers
    CacheNode* prev;
    CacheNode* next;

    // Hash Table Chaining Pointer
    CacheNode* h_next;

    CacheNode(const std::string& k, const std::string& v)
        : key(k), value(v), expire_at(0), prev(nullptr), next(nullptr), h_next(nullptr) {}
};

// ---------------------------------------------------------
// CUSTOM HASH TABLE + LRU CACHE
// ---------------------------------------------------------
class LRUHashTable {
private:
    std::vector<CacheNode*> buckets;
    size_t capacity;
    size_t size;
    size_t max_memory_items;

    // LRU Head and Tail (Dummy nodes for simplicity)
    CacheNode* head;
    CacheNode* tail;

    size_t hash_func(const std::string& key) const {
        size_t hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + c; // djb2
        }
        return hash % capacity;
    }

    void rehash() {
        size_t new_capacity = capacity * 2;
        std::vector<CacheNode*> new_buckets(new_capacity, nullptr);

        CacheNode* curr = head->next;
        while (curr != tail) {
            size_t idx = hash_func(curr->key) % new_capacity;
            CacheNode* next_node = curr->next;
            
            curr->h_next = new_buckets[idx];
            new_buckets[idx] = curr;
            
            curr = next_node;
        }
        buckets = std::move(new_buckets);
        capacity = new_capacity;
    }

    void remove_from_list(CacheNode* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void add_to_front(CacheNode* node) {
        node->next = head->next;
        node->prev = head;
        head->next->prev = node;
        head->next = node;
    }

    void evict_lru() {
        if (size == 0) return;
        CacheNode* lru = tail->prev;
        remove(lru->key);
    }

public:
    LRUHashTable(size_t max_items = 100000) : capacity(1024), size(0), max_memory_items(max_items) {
        buckets.resize(capacity, nullptr);
        head = new CacheNode("", "");
        tail = new CacheNode("", "");
        head->next = tail;
        tail->prev = head;
    }

    ~LRUHashTable() {
        CacheNode* curr = head;
        while (curr) {
            CacheNode* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    void set(const std::string& key, const std::string& value, long long expire_at = 0) {
        size_t idx = hash_func(key);
        CacheNode* curr = buckets[idx];
        
        while (curr) {
            if (curr->key == key) {
                curr->value = value;
                curr->expire_at = expire_at;
                remove_from_list(curr);
                add_to_front(curr);
                return;
            }
            curr = curr->h_next;
        }

        if (size >= max_memory_items) {
            evict_lru();
            idx = hash_func(key); // Recalculate
        }

        CacheNode* new_node = new CacheNode(key, value);
        new_node->expire_at = expire_at;
        
        new_node->h_next = buckets[idx];
        buckets[idx] = new_node;

        add_to_front(new_node);

        size++;
        if (size > capacity * 0.75) {
            rehash();
        }
    }

    CacheNode* get(const std::string& key) {
        size_t idx = hash_func(key);
        CacheNode* curr = buckets[idx];
        
        while (curr) {
            if (curr->key == key) {
                auto now = std::chrono::system_clock::now().time_since_epoch();
                long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                if (curr->expire_at > 0 && ms > curr->expire_at) {
                    remove(key);
                    return nullptr;
                }
                remove_from_list(curr);
                add_to_front(curr);
                return curr;
            }
            curr = curr->h_next;
        }
        return nullptr;
    }

    bool remove(const std::string& key) {
        size_t idx = hash_func(key);
        CacheNode* curr = buckets[idx];
        CacheNode* prev = nullptr;

        while (curr) {
            if (curr->key == key) {
                if (prev) {
                    prev->h_next = curr->h_next;
                } else {
                    buckets[idx] = curr->h_next;
                }
                remove_from_list(curr);
                delete curr;
                size--;
                return true;
            }
            prev = curr;
            curr = curr->h_next;
        }
        return false;
    }

    std::vector<std::pair<std::string, std::string>> snapshot() const {
        std::vector<std::pair<std::string, std::string>> dump;
        CacheNode* curr = head->next;
        while (curr != tail) {
            dump.push_back({curr->key, curr->value});
            curr = curr->next;
        }
        return dump;
    }
};

// ---------------------------------------------------------
// SKIP LIST (ZSET)
// ---------------------------------------------------------
struct SkipNode {
    std::string value;
    double score;
    std::vector<SkipNode*> forward;
    SkipNode(std::string v, double s, int level) : value(v), score(s), forward(level, nullptr) {}
};

class SkipList {
private:
    int MAX_LEVEL = 16;
    float P = 0.5;
    SkipNode* header;
    int level;

    int random_level() {
        int lvl = 1;
        while (((float)rand() / RAND_MAX) < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

public:
    SkipList() : level(1) {
        header = new SkipNode("", -1, MAX_LEVEL);
    }
    
    void insert(std::string value, double score) {
        std::vector<SkipNode*> update(MAX_LEVEL, nullptr);
        SkipNode* current = header;
        
        for (int i = level - 1; i >= 0; i--) {
            while (current->forward[i] && current->forward[i]->score < score) {
                current = current->forward[i];
            }
            update[i] = current;
        }
        
        current = current->forward[0];
        
        if (current == nullptr || current->value != value) {
            int lvl = random_level();
            if (lvl > level) {
                for (int i = level; i < lvl; i++) {
                    update[i] = header;
                }
                level = lvl;
            }
            
            SkipNode* newNode = new SkipNode(value, score, lvl);
            for (int i = 0; i < lvl; i++) {
                newNode->forward[i] = update[i]->forward[i];
                update[i]->forward[i] = newNode;
            }
        }
    }
    
    std::vector<std::string> range(double min_score, double max_score) {
        std::vector<std::string> result;
        SkipNode* current = header;
        for (int i = level - 1; i >= 0; i--) {
            while (current->forward[i] && current->forward[i]->score < min_score) {
                current = current->forward[i];
            }
        }
        current = current->forward[0];
        while (current && current->score <= max_score) {
            result.push_back(current->value);
            current = current->forward[0];
        }
        return result;
    }
};
