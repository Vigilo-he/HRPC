#ifndef HRPC_LOGGER_H
#define HRPC_LOGGER_H

#include <glog/logging.h>
#include <string>

class hrpc_logger {
public:
    explicit hrpc_logger(const std::string& argv0) {
        google::InitGoogleLogging(argv0.c_str());
        FLAGS_colorlogtostderr = true;
        FLAGS_logtostderr = true;
    }
    ~hrpc_logger() {
        google::ShutdownGoogleLogging();
    }
    
    static void info(const std::string& message) {
        LOG(INFO) << message;
    }
    static void warning(const std::string& message) {
        LOG(WARNING) << message;
    }
    static void error(const std::string& message) {
        LOG(ERROR) << message;
    }
    static void fatal(const std::string& message) {
        LOG(FATAL) << message;
    }

private:
    hrpc_logger(const hrpc_logger&) = delete;
    hrpc_logger& operator = (const hrpc_logger&) = delete;
};

#endif