#include "consistent_hash.h"
#include <iostream>
#include <chrono>

// CRC32 查找表
static const uint32_t k_crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    // ... 完整表保持不变 ...
};

uint32_t consistent_hash::hash_func(const std::string& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < data.length(); ++i) {
        crc = k_crc32_table[(crc ^ static_cast<uint8_t>(data[i])) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

consistent_hash::consistent_hash(hash_config cfg)
    : m_config(cfg), m_total_requests(0), m_stop_balancer(false) {
    start_balancer();
}

consistent_hash::~consistent_hash() {
    m_stop_balancer = true;
    if (m_balancer_thread.joinable()) {
        m_balancer_thread.join();
    }
}

void consistent_hash::add_node_internal(const std::string& node, int replicas) {
    for (int i = 0; i < replicas; ++i) {
        std::string virtual_name = node + "#" + std::to_string(i);
        uint32_t h = hash_func(virtual_name);
        m_keys.push_back(h);
        m_ring[h] = node;
    }
    m_node_replicas[node] = replicas;
    if (m_node_counts.find(node) == m_node_counts.end()) {
        m_node_counts[node] = std::make_shared<std::atomic<long long>>(0);
    }
}

void consistent_hash::add_nodes(const std::vector<std::string>& nodes) {
    std::unique_lock<std::shared_timed_mutex> lock(m_rw_mtx);
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].empty()) {
            add_node_internal(nodes[i], m_config.replicas);
        }
    }
    std::sort(m_keys.begin(), m_keys.end());
}

void consistent_hash::remove_node(const std::string& node) {
    std::unique_lock<std::shared_timed_mutex> lock(m_rw_mtx);
    auto it_rep = m_node_replicas.find(node);
    if (it_rep == m_node_replicas.end()) return;

    int replicas = it_rep->second;
    for (int i = 0; i < replicas; ++i) {
        uint32_t h = hash_func(node + "#" + std::to_string(i));
        m_ring.erase(h);
        auto it_vec = std::remove(m_keys.begin(), m_keys.end(), h);
        m_keys.erase(it_vec, m_keys.end());
    }
    m_node_replicas.erase(node);
    m_node_counts.erase(node);
}

std::string consistent_hash::get_target_node(const std::string& key) {
    std::string node;
    std::shared_ptr<std::atomic<long long>> counter_ptr;

    {
        std::shared_lock<std::shared_timed_mutex> lock(m_rw_mtx);
        if (m_keys.empty()) return "";

        uint32_t h = hash_func(key);
        auto it = std::lower_bound(m_keys.begin(), m_keys.end(), h);
        if (it == m_keys.end()) it = m_keys.begin();

        node = m_ring[*it];
        counter_ptr = m_node_counts[node];
    }

    thread_local int sample_counter = 0;
    if (++sample_counter >= 100) {
        if (counter_ptr) {
            counter_ptr->fetch_add(100, std::memory_order_relaxed);
        }
        m_total_requests.fetch_add(100, std::memory_order_relaxed);
        sample_counter = 0;
    }

    return node;
}

void consistent_hash::start_balancer() {
    m_balancer_thread = std::thread([this]() {
        while (!m_stop_balancer) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!m_stop_balancer) check_and_rebalance();
        }
    });
}

void consistent_hash::check_and_rebalance() {
    if (m_total_requests.load() < 500) return;

    std::unique_lock<std::shared_timed_mutex> lock(m_rw_mtx);
    if (m_node_replicas.empty()) return;

    double avg = static_cast<double>(m_total_requests.load()) / m_node_replicas.size();
    double max_skew = 0.0;

    for (auto const& pair : m_node_counts) {
        double skew = std::abs(static_cast<double>(pair.second->load()) - avg) / (avg > 0 ? avg : 1.0);
        if (skew > max_skew) max_skew = skew;
    }

    if (max_skew > m_config.balance_threshold) {
        rebalance_logic();
    }
}

void consistent_hash::rebalance_logic() {
    double avg = static_cast<double>(m_total_requests.load()) / m_node_replicas.size();
    auto old_replicas_map = m_node_replicas;
    auto counts_snapshot = m_node_counts;
    m_node_counts.clear();
    m_total_requests.store(0);

    for (auto const& pair : counts_snapshot) {
        const std::string& node = pair.first;
        long long count = pair.second->load();
        int old_rep = old_replicas_map[node];

        double ratio = (avg > 0) ? (static_cast<double>(count) / avg) : 1.0;
        int new_rep;
        if (ratio > 1.0)
            new_rep = static_cast<int>(std::round(old_rep / ratio));
        else
            new_rep = static_cast<int>(std::round(old_rep * (2.0 - ratio)));

        new_rep = std::max(m_config.min_replicas, std::min(m_config.max_replicas, new_rep));

        if (new_rep != old_rep) {
            for (int i = 0; i < old_rep; ++i) {
                uint32_t h = hash_func(node + "#" + std::to_string(i));
                m_ring.erase(h);
                auto it_v = std::remove(m_keys.begin(), m_keys.end(), h);
                m_keys.erase(it_v, m_keys.end());
            }
            add_node_internal(node, new_rep);
        }
    }

    std::sort(m_keys.begin(), m_keys.end());
}

std::unordered_map<std::string, double> consistent_hash::get_stats() {
    std::shared_lock<std::shared_timed_mutex> lock(m_rw_mtx);
    std::unordered_map<std::string, double> stats;
    long long total = m_total_requests.load();
    if (total == 0) return stats;
    for (auto const& pair : m_node_counts) {
        stats[pair.first] = static_cast<double>(pair.second->load()) / total;
    }
    return stats;
}

void consistent_hash::update_nodes(const std::vector<std::string>& nodes) {
    std::unique_lock<std::shared_timed_mutex> lock(m_rw_mtx);

    m_keys.clear();
    m_ring.clear();
    m_node_replicas.clear();
    m_node_counts.clear();
    m_total_requests.store(0);

    for (size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].empty()) {
            add_node_internal(nodes[i], m_config.replicas);
        }
    }

    std::sort(m_keys.begin(), m_keys.end());

    std::cout << "[ConsistentHash] 节点列表已全量更新，共计 " << nodes.size() << " 个节点。" << std::endl;
}