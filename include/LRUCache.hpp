#ifndef LRUCACHE_HPP
#define LRUCACHE_HPP

#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <optional>

namespace miniCDN{

class LRUCache{
public:
    LRUCache(size_t capacity);

    // keeping std::optional to handle cache miss safely without throwing exceptions or returning empty strings
    std::optional<std::string> get(const std::string& key);

    void put(const std::string& key, const std::string& value);

private:
    size_t m_capacity;
    
    // shared_mutex (reader-writer lock)
    mutable std::shared_mutex m_mutex;

    // Doubly linked list to store keys and values
    std::list<std::pair<std::string, std::string>> m_list;
    
    // Hash map mapping a key to the exact iterator in the list
    std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>::iterator> m_map;
};

} // namespace miniCDN

#endif