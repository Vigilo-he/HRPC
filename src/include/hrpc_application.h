#ifndef HRPC_APPLICATION_H
#define HRPC_APPLICATION_H

#include "hrpc_config.h"

class hrpc_application {
public:
    static void init(int argc, char** argv);
    static hrpc_application& get_instance();
    static hrpc_config& get_config();
    
private:
    static hrpc_config m_config;
    hrpc_application();
    ~hrpc_application();
    hrpc_application(const hrpc_application&) = delete;
    hrpc_application& operator = (hrpc_application&) = delete;
};

#endif