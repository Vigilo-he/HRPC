#ifndef HRPC_CONNECT_POOL_H
#define HRPC_CONNECT_POOL_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <memory>
#include "lock_free_queue.h"


struct connection_bucket {
    std::unique_ptr<mpmc_queue<int>> free_fds;
    std::atomic<int> active_count;
    connection_bucket(size_t capacity)
        : free_fds(new mpmc_queue<int>(capacity)), active_count(0) {}
};

class hrpc_connect_pool {
public:
    static hrpc_connect_pool &get_instance();

    int borrow_connection(const std::string &ip, uint16_t port);

    void return_connection(const std::string &ip, uint16_t port, int fd, bool is_bad = false);

    void warm_up(const std::string &ip, uint16_t port, int count);

private:
    hrpc_connect_pool();
    ~hrpc_connect_pool();

    int m_max_conn_per_node;
    std::unordered_map<std::string, connection_bucket *> m_pools;
    std::mutex m_global_mtx;
};


#endif
