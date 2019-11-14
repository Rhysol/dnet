#pragma once
#include <spdlog/spdlog.h>

extern std::shared_ptr<spdlog::logger> net_logger;

#define LOGD(msg, ...) net_logger->debug(msg, ##__VA_ARGS__)
#define LOGI(msg, ...) net_logger->info(msg, ##__VA_ARGS__)
#define LOGW(msg, ...) net_logger->warn(msg, ##__VA_ARGS__)
#define LOGE(msg, ...) net_logger->error(msg, ##__VA_ARGS__)
