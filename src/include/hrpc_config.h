#ifndef HRPC_CONFIG_H
#define HRPC_CONFIG_H

#include <unordered_map>
#include <string>

class hrpc_config {
public:
    void load_config_file(const std::string &config_file);
    std::string load(const std::string &key);

private:
    std::unordered_map<std::string, std::string> config_map;
    void trim(std::string &read_buf);
};

#endif