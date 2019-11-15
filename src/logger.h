#pragma once
#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> net_logger;

#define LOGD(msg, ...) net_logger->debug("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGI(msg, ...) net_logger->info("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(msg, ...) net_logger->warn("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(msg, ...) net_logger->error("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

