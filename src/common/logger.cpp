#include "logger.h"

#include <glog/flags.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace recommendation {
namespace {

std::string g_resolved_log_dir;

std::filesystem::path FindEngineRoot() {
  if (const char *root = std::getenv("RE_ENGINE_ROOT")) {
    return std::filesystem::path(root);
  }

  std::filesystem::path dir = std::filesystem::current_path();
  for (int depth = 0; depth < 32 && !dir.empty() && dir != dir.root_path();
       ++depth) {
    if (std::filesystem::exists(dir / "WORKSPACE") ||
        std::filesystem::exists(dir / "MODULE.bazel")) {
      return dir;
    }
    dir = dir.parent_path();
  }

  if (const char *home = std::getenv("HOME")) {
    return std::filesystem::path(home) / ".recommendation_engine";
  }
  return std::filesystem::current_path();
}

std::filesystem::path ResolveLogDir(const char *log_dir) {
  std::filesystem::path path(log_dir);
  if (path.is_absolute()) {
    return path;
  }
  return FindEngineRoot() / path;
}

void WriteLogLocationHint(const std::string &server_name,
                          const std::filesystem::path &log_dir) {
  const std::filesystem::path engine_root = FindEngineRoot();
  std::error_code ec;
  const std::filesystem::path logs_root = engine_root / "logs";
  std::filesystem::create_directories(logs_root, ec);

  std::ofstream out(logs_root / "WHERE_ARE_LOGS.txt", std::ios::app);
  if (!out) {
    return;
  }
  out << server_name << " -> " << log_dir.string() << '\n';
}

} // namespace

bool InitLogging(int /*argc*/, char ** /*argv*/, const LogOptions &options) {
  if (!options.server_name || !options.log_dir) {
    std::cerr << "[logger] InitLogging 失败：server_name 与 log_dir 不能为空\n";
    return false;
  }

  g_resolved_log_dir = ResolveLogDir(options.log_dir).string();

  std::error_code ec;
  std::filesystem::create_directories(g_resolved_log_dir, ec);
  if (ec) {
    std::cerr << "[logger] 无法创建日志目录 " << g_resolved_log_dir << ": "
              << ec.message() << '\n';
    return false;
  }

  WriteLogLocationHint(options.server_name, g_resolved_log_dir);

  FLAGS_log_dir = g_resolved_log_dir;
  FLAGS_logtostderr = false;
  FLAGS_alsologtostderr = options.also_log_to_stderr;
  FLAGS_minloglevel = options.min_log_level;
  FLAGS_max_log_size = options.max_log_size_mb;

  google::InitGoogleLogging(options.server_name);
  google::EnableLogCleaner(options.log_retention_days);

  LOG(INFO) << "日志已初始化 server=" << options.server_name
            << " log_dir=" << g_resolved_log_dir
            << " max_log_size_mb=" << options.max_log_size_mb
            << " retention_days=" << options.log_retention_days
            << "（glog 按大小滚动，过期自动清理）";
  return true;
}

const std::string &ResolvedLogDir() { return g_resolved_log_dir; }

void ShutdownLogging() { google::ShutdownGoogleLogging(); }

} // namespace recommendation
