#pragma once
#include <QHash>
#include <QList>
#include <QPair>
#include <functional>

// LRU 缓存模板：容量固定，淘汰最久未使用的条目
template<typename Key, typename Value>
class LRUCache
{
public:
    explicit LRUCache(int capacity) : m_capacity(capacity) {}

    bool contains(const Key &key) const { return m_map.contains(key); }
    int  size()     const { return m_map.size(); }
    int  capacity() const { return m_capacity; }

    // 获取值，命中则移到头部，未命中返回 false
    bool get(const Key &key, Value &out)
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return false;
        // QLinkedList 无 moveToFront：复制节点 → 删除 → 头插 → 更新迭代器
        QPair<Key, Value> pair = *it.value();
        m_list.erase(it.value());
        m_list.prepend(pair);
        m_map[key] = m_list.begin();
        out = pair.second;
        return true;
    }

    // 插入/更新，返回被淘汰的条目（若有）
    bool put(const Key &key, const Value &value, Key *evictedKey = nullptr, Value *evictedValue = nullptr)
    {
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            m_list.erase(it.value());
            m_map.remove(key);
        }
        bool evicted = false;
        if (m_map.size() >= m_capacity) {
            auto last = m_list.end(); --last;
            if (evictedKey)   *evictedKey   = last->first;
            if (evictedValue) *evictedValue = last->second;
            m_map.remove(last->first);
            m_list.erase(last);
            evicted = true;
        }
        m_list.prepend(qMakePair(key, value));
        m_map.insert(key, m_list.begin());
        return evicted;
    }

    // 移除特定 key
    bool remove(const Key &key)
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return false;
        m_list.erase(it.value());
        m_map.erase(it);
        return true;
    }

    // 遍历所有 key
    QList<Key> keys() const
    {
        QList<Key> result;
        for (auto &p : m_list) result.append(p.first);
        return result;
    }

private:
    int m_capacity;
    using ListType = QList<QPair<Key, Value>>;
    ListType m_list;
    QHash<Key, typename ListType::iterator> m_map;
};
