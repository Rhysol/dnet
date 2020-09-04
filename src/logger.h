#pragma once
#include <spdlog/spdlog.h>

#define LOGD(msg, ...) m_net_config->logger->debug("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGI(msg, ...) m_net_config->logger->info("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(msg, ...) m_net_config->logger->warn("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(msg, ...) m_net_config->logger->error("{}({}):{}() " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
