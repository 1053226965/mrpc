#include "common/debug/log.hpp"

std::shared_ptr<spdlog::logger> g_logger = spdlog::basic_logger_mt("g_logger", "logs/mrpc.log");

void init_g_logger()
{
  // %e Millisecond part of the current second 000-999
  g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");
  g_logger->flush_on(spdlog::level::level_enum::info);
}

