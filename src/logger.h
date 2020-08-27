#pragma once
#include <spdlog/spdlog.h>

namespace dnet
{

extern std::shared_ptr<spdlog::logger> net_logger;

}

#define LOGD(msg, ...) dnet::net_logger->debug("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGI(msg, ...) dnet::net_logger->info("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(msg, ...) dnet::net_logger->warn("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(msg, ...) dnet::net_logger->error("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
