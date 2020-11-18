#pragma once
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

extern std::shared_ptr<spdlog::logger> g_logger;
void init_g_logger();

#define DETAIL_LOG

#ifdef DETAIL_LOG

#define DETAIL_LOG_INFO(format, ...) \
  g_logger->info(format, ##__VA_ARGS__)

#define DETAIL_LOG_WARN(format, ...) \
  g_logger->warn(format, ##__VA_ARGS__)

#define DETAIL_LOG_ERROR(format, ...) \
  g_logger->error(format, ##__VA_ARGS__)

#else
#define DETAIL_LOG_INFO(format, ...)
#define DETAIL_LOG_WARN(format, ...)
#define DETAIL_LOG_ERROR(format, ...)
#endif
