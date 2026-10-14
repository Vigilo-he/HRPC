#include "hrpc_connect_pool.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include<netinet/tcp.h>
#include<arpa/inet.h>
#include <string>
#include <sys/socket.h>


static const int MAX_CONN_SIZE = 1024;

hrpc_connect_pool &hrpc_connect_pool::get_instance() {
    static hrpc_connect_pool instance;
    return instance;
}

hrpc_connect_pool::hrpc_connect_pool() : m_max_conn_per_node(MAX_CONN_SIZE) {}

hrpc_connect_pool::~hrpc_connect_pool() {
    std::lock_guard<std::mutex> lock(m_global_mtx);
    for (auto &pair : m_pools) {
        int fd;
        while (pair.second->free_fds->pop(fd)) {
            close(fd);
        }
        delete pair.second;
    } 
}

void hrpc_connect_pool::warm_up(const std::string &ip, uint16_t port, int count) {
    std::string key = ip + ":" + std::to_string(port);
    connection_bucket *bucket = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(m_global_mtx);
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            bucket = new connection_bucket(MAX_CONN_SIZE);
            m_pools[key] = bucket;
        } else {
            bucket = it->second;
        }
    }

    int success_count = 0;
    for (int i = 0; i < count; i++) {
        if (bucket->active_count.load() >= m_max_conn_per_node) break;

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) continue;

        int opt = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());

        if (connect(fd, (sockaddr *)&addr, sizeof(addr)) == 0) {
            bucket->active_count.fetch_add(1);
            if (!bucket->free_fds->push(fd)) {
                close(fd);
                bucket->active_count.fetch_sub(1);
            } else {
                success_count++;
            }
        } else {
            close(fd);
        }
    }
}

int hrpc_connect_pool::borrow_connection(const std::string &ip, uint16_t port) {
    std::string key = ip + ":" + std::to_string(port);

    connection_bucket *bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_global_mtx);
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            bucket = new(connection_bucket(MAX_CONN_SIZE));
        } else {
            bucket = it->second;
        }
    }

    int fd = -1;
    if (bucket->free_fds->pop(fd)) {
        return fd;
    }
    
    if (bucket->active_count.load(std::memory_order_relaxed) < m_max_conn_per_node) {
        if (bucket->active_count.fetch_add(1) < m_max_conn_per_node) {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
            
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());

            if (connect(fd, (sockaddr *)&addr, sizeof(addr)) == 0) {
                return fd;
            } else {
                close(fd);
                bucket->active_count.fetch_sub(1);
                return -1;
            }
        } else {
            bucket->active_count.fetch_sub(1);
        }
    }
    return -1;
}

void hrpc_connect_pool::return_connection(const std::string &ip, uint16_t port, int fd, bool is_bad) {
    std::string key = ip + ":" + std::to_string(port);

    connection_bucket *bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_global_mtx);
        auto it = m_pools.find(key);
        if (it != m_pools.end()) bucket = it->second;
    }

    if (!bucket) {
        close(fd);
        return;
    }

    if (is_bad) {
        close(fd);
        bucket->active_count.fetch_sub(1);
    } else {
        if (!bucket->free_fds->push(fd)) {
            close(fd);
            bucket->active_count.fetch_sub(1);
        }
    }
}
