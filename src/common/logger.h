#pragma once

#include <glog/logging.h>

#include <string>

// 进程级 glog 初始化：每个可执行文件在 main 开头调用一次 InitLogging。
//
// 使用 glog 默认落盘：日志在 <log_dir>/ 下，文件名含级别与时间戳；
// 单文件超过 max_log_size_mb 自动滚动；超过 log_retention_days 的旧文件自动删除。
//
// log_dir 可为相对路径（相对项目根）或绝对路径。项目根查找顺序：
//   1. 环境变量 RE_ENGINE_ROOT
//   2. 从当前工作目录向上找 WORKSPACE / MODULE.bazel
//   3. 回退到 $HOME/.recommendation_engine/

namespace recommendation {

constexpr char kRankingLogDir[] = "logs/ranking";
constexpr char kMainServerLogDir[] = "logs/mainserver";

struct LogOptions {
  const char *server_name = nullptr;
  const char *log_dir = nullptr;
  bool also_log_to_stderr = true;
  int min_log_level = 0; // glog: 0=INFO, 1=WARNING, 2=ERROR, 3=FATAL
  int max_log_size_mb = 100;
  unsigned int log_retention_days = 7;
};

bool InitLogging(int argc, char **argv, const LogOptions &options);
const std::string &ResolvedLogDir();
void ShutdownLogging();

} // namespace recommendation
