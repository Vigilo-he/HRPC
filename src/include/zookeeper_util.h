#ifndef ZOOKEEPER_UTIL_H
#define ZOOKEEPER_UTIL_H

#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <vector>
#include <string>


class zk_client {
public:
    zk_client();
    ~zk_client();
    void start();
    void create(const char* path, const char* data, int datalen, int state = 0);
    std::string get_data(const char* path);
    std::vector<std::string> get_children(const char* path, watcher_fn fn, void* cbContext);

    private:
    zhandle_t* m_zhandle;
};

#endif