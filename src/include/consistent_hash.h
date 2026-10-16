#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <cmath>
#include <mutex>
#include <memory>

// 一致性哈希配置
struct hash_config {
    int replicas;              // 初始每个节点的虚拟节点数
    int min_replicas;          // 最小虚拟节点数限制
    int max_replicas;          // 最大虚拟节点数限制
    double balance_threshold;  // 负载不均阈值
};

class consistent_hash {
public:
    consistent_hash(hash_config cfg = {10, 5, 200, 0.25});
    ~consistent_hash();

    void add_nodes(const std::vector<std::string>& nodes);
    void remove_node(const std::string& node);
    std::string get_target_node(const std::string& key);
    void update_nodes(const std::vector<std::string>& nodes);
    std::unordered_map<std::string, double> get_stats();

private:
    uint32_t hash_func(const std::string& data);
    void add_node_internal(const std::string& node, int replicas);
    void start_balancer();
    void check_and_rebalance();
    void rebalance_logic();

private:
    hash_config m_config;

    std::vector<uint32_t> m_keys;
    std::unordered_map<uint32_t, std::string> m_ring;

    std::unordered_map<std::string, int> m_node_replicas;
    std::atomic<long long> m_total_requests;

    std::unordered_map<std::string, std::shared_ptr<std::atomic<long long>>> m_node_counts;
    mutable std::shared_timed_mutex m_rw_mtx;
    std::thread m_balancer_thread;
    std::atomic<bool> m_stop_balancer;
};

#endif