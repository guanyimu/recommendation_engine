#pragma once

#include "config.h"
#include "logger.h"

#include <memory>
#include <string>

namespace recommendation {

// 服务端 bind 地址：优先环境变量 RE_LISTEN_ADDRESS（多实例 + Nginx LB），否则读 conf。
std::string ResolveListenAddress(
    const std::shared_ptr<const ConfigSnapshot> &snap,
    const char *config_key);

bool ListenAddressFromEnv();

// 多实例时 RE_INSTANCE_NAME=ranking-1 → logs/ranking-1/ 与独立 glog 前缀。
void ApplyInstanceLogging(LogOptions &opts, const char *default_server_name,
                          const char *default_log_dir);

}  // namespace recommendation
