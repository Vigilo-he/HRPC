#include "hrpc_config.h"
#include <fstream>
#include <cstdlib>

void hrpc_config::load_config_file(const std::string &config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        exit(EXIT_FAILURE);
    }

    std::string line;
    while (std::getline(file, line)) {
        trim(line);

        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        trim(key);
        trim(value);

        config_map[key] = value;
    }
}

std::string hrpc_config::load(const std::string &key) {
    auto it = config_map.find(key);
    if (it == config_map.end()) {
        return "";
    }
    return it->second;
}

void hrpc_config::trim(std::string &str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        str.clear();
        return;
    }

    auto end = str.find_last_not_of("\t\r\n");
    str = str.substr(start, end - start + 1);
}