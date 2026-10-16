#include "service_discovery.h"
#include <mutex>
#include <shared_mutex>
#include <zookeeper/zookeeper.h>

service_discovery& service_discovery::get_instance() {
    static service_discovery instance;
    return instance;
}

void service_discovery::init() {
    m_zkclient.start();
}

std::string service_discovery::get_target_node(const std::string& service_name, const std::string& key) {
    {
        std::shared_lock<std::shared_timed_mutex> read_lock(m_rw_mtx);
        if (m_chash_map.find(service_name) != m_chash_map.end()) {
            return m_chash_map[service_name]->get_target_node(key);
        } 
    }

    std::string path = "/" + service_name;
    std::vector<std::string> new_nodes = m_zkclient.get_children(path.c_str(), watcher_callback, this);

    {
        std::unique_lock<std::shared_timed_mutex> write_lock(m_rw_mtx);
        if (m_chash_map.find(service_name) == m_chash_map.end()) {
            m_chash_map[service_name] = std::unique_ptr<consistent_hash>(new consistent_hash());
            m_chash_map[service_name]->update_nodes(new_nodes);
            m_nodes_cache[service_name] = new_nodes;
        }
        return m_chash_map[service_name]->get_target_node(key);
    }
}

void service_discovery::watcher_callback(zhandle_t* zh, int type, int state, const char* path, void* watcher_ctx) {
    if (type == ZOO_CHILD_EVENT) {
        service_discovery* sd = static_cast<service_discovery*>(watcher_ctx);

        std::string path_str = path;
        std::string service_name = path_str.substr(1);

        std::vector<std::string> new_nodes = sd->m_zkclient.get_children(path_str.c_str(), watcher_callback, sd);

        std::unique_lock<std::shared_timed_mutex> lock(sd->m_rw_mtx);
        if (sd->m_chash_map.find(service_name) != sd->m_chash_map.end()) {
            sd->m_chash_map[service_name]->update_nodes(new_nodes);
        }
        sd->m_nodes_cache[service_name] = new_nodes;
    }
}