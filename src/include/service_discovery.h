#ifndef SERVICE_DISCOVERY_H
#define SERVICE_DISCOVERY_H

#include "zookeeper_util.h"
#include "consistent_hash.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

class service_discovery {
public:
    static service_discovery& get_instance();
    void init();
    std::string get_target_node(const std::string& service_name, const std::string& key);

private:
    service_discovery();
    ~service_discovery();
    service_discovery(const service_discovery&) = delete;
    service_discovery& operator = (const service_discovery&) = delete;

    zk_client m_zkclient;
    std::unordered_map<std::string, std::unique_ptr<consistent_hash>> m_chash_map;
    std::unordered_map<std::string, std::vector<std::string>> m_nodes_cache;

    std::shared_timed_mutex m_rw_mtx;

    static void watcher_callback(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);
    void update_cache_locked(const std::string &service_name, const std::string &path);
};

#endif