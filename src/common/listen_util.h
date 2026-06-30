#pragma once

#include "logger.h"

namespace recommendation {

// 多实例日志目录：RE_INSTANCE_NAME=ranking-1 → logs/ranking-1/
void ApplyInstanceLogging(LogOptions &opts, const char *default_server_name,
                          const char *default_log_dir);

}  // namespace recommendation
