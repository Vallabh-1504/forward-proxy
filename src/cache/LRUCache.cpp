#include "LRUCache.hpp"

namespace miniCDN{

LRUCache::LRUCache(size_t capacity) : m_capacity(capacity){
    if(m_capacity == 0){
        m_capacity = 1; // Prevent zero-capacity edge cases
    }
}

std::optional<std::string> LRUCache::get(const std::string& key){
    // lock: Touching the list
    std::lock_guard lock(m_mutex);

    auto it = m_map.find(key);
    if(it == m_map.end()){
        return std::nullopt;
    }

    // Moving accessed node to front
    m_list.splice(m_list.begin(), m_list, it->second);
    
    return it->second->second;
}

void LRUCache::put(const std::string& key, const std::string& value){
    // lock: Touching the list 
    std::lock_guard lock(m_mutex);

    // Check if key exists
    auto it = m_map.find(key);
    if(it != m_map.end()){
        // Update existing value and move to front
        it->second->second = value;
        m_list.splice(m_list.begin(), m_list, it->second);
        return;
    }

    // key exists, change the value and put at front
    m_list.emplace_front(key, value);
    m_map[key] = m_list.begin();

    // Check capacity and evict the Least Recently Used
    if(m_map.size() > m_capacity){
        auto last = m_list.back();
        m_map.erase(last.first);
        m_list.pop_back();
    }
}

} // namespace miniCDN