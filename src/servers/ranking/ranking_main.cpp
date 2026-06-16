#include "gated_config_reloader.h"
#include "logger.h"
#include "ranking_grpc_server.h"
#include <chrono>
#include <csignal>
#include <thread>

namespace {

constexpr char kConfigPath[] = "configs/ranking.conf";

std::atomic<bool> g_reload_requested{false};
std::atomic<bool> g_shutdown{false};

void OnSigHup(int /*signo*/) { g_reload_requested.store(true); }

void OnSigInt(int /*signo*/) { g_shutdown.store(true); }

} // namespace

int main(int argc, char **argv) {
  recommendation::LogOptions log_opts;
  log_opts.server_name = "ranking";
  log_opts.log_dir = recommendation::kRankingLogDir;
  if (!recommendation::InitLogging(argc, argv, log_opts)) {
    return 1;
  }

  if (!recommendation::GatedConfig::Instance().Init(kConfigPath)) {
    LOG(ERROR) << "[ranking] 无法加载配置: " << kConfigPath;
    recommendation::ShutdownLogging();
    return 1;
  }

  recommendation::GatedConfig &config = recommendation::GatedConfig::Instance();

  std::string ranking_address;
  if (!config.RequireString("ranking_address", ranking_address)) {
    LOG(ERROR) << "[ranking] conf 缺少 ranking_address 或值为空";
    recommendation::ShutdownLogging();
    return 1;
  }

  int recommend_count = 0;
  if (!config.RequireInt("recommend_count", recommend_count)) {
    LOG(ERROR) << "[ranking] conf 缺少 recommend_count 或值非法";
    recommendation::ShutdownLogging();
    return 1;
  }

  recommendation::RankingGrpcServer server(ranking_address);

  std::signal(SIGHUP, OnSigHup);
  std::signal(SIGINT, OnSigInt);

  std::thread control_thread([&server] {
    while (!g_shutdown.load()) {
      if (g_reload_requested.exchange(false)) {
        server.Reload();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    server.Shutdown();
  });

  LOG(INFO) << "[ranking] 监听: " << server.address()
            << " recommend_count=" << recommend_count
            << "（修改 conf 后 kill -HUP <pid> 触发 Reload）";

  if (!server.Run()) {
    g_shutdown.store(true);
    if (control_thread.joinable()) {
      control_thread.join();
    }
    recommendation::ShutdownLogging();
    return 1;
  }

  g_shutdown.store(true);
  if (control_thread.joinable()) {
    control_thread.join();
  }

  recommendation::ShutdownLogging();
  return 0;
}
