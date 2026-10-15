#include "hrpc_application.h"
#include "hrpc_config.h"
#include <cstdlib>
#include <iostream>
#include <unistd.h>

void hrpc_application::init(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "格式: command -i <配置文件路径>" << std::endl;
        exit(EXIT_FAILURE);
    }

    int o;
    std::string config_file;
    while ((o = getopt(argc, argv, "i:")) != -1) {
        switch (o) {
            case 'i':
                config_file = optarg;
                break;
            case '?':
                std::cout << "格式: command -i <配置文件路径>" << std::endl;
                exit(EXIT_FAILURE);
                break;
            case ':':
                std::cout << "格式: command -i <配置文件路径>" << std::endl;
                exit(EXIT_FAILURE);
                break;
            default:
                break;
        }

        m_config.load_config_file(config_file.c_str());
    }
}

hrpc_application& hrpc_application::get_instance() {
    static hrpc_application m_application;
    return m_application;
}

hrpc_config& hrpc_application::get_config() {
    return m_config;
}
