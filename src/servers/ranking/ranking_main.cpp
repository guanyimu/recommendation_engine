#include "gated_config_reloader.h"
#include "ranking_grpc_server.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {

constexpr char kConfigPath[] = "configs/ranking.conf";

std::atomic<bool> g_reload_requested{false};
std::atomic<bool> g_shutdown{false};

void OnSigHup(int /*signo*/) { g_reload_requested.store(true); }

void OnSigInt(int /*signo*/) { g_shutdown.store(true); }

}  // namespace

int main() {
  if (!recommendation::GatedConfig::Init(kConfigPath)) {
    std::cerr << "[ranking] 无法加载配置: " << kConfigPath << '\n';
    return 1;
  }

  recommendation::GatedConfig config;

  std::string ranking_address;
  if (!config.RequireString("ranking_address", &ranking_address)) {
    std::cerr << "[ranking] conf 缺少 ranking_address 或值为空\n";
    return 1;
  }

  std::size_t recommend_count = 0;
  if (!config.RequireSizeT("recommend_count", &recommend_count)) {
    std::cerr << "[ranking] conf 缺少 recommend_count 或值非法\n";
    return 1;
  }

  recommendation::RankingGrpcServer server(config, ranking_address);

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

  std::cout << "[ranking] 监听: " << server.address()
            << " recommend_count=" << recommend_count
            << "（修改 conf 后 kill -HUP <pid> 触发 Reload）\n";

  server.Run();

  g_shutdown.store(true);
  if (control_thread.joinable()) {
    control_thread.join();
  }
  return 0;
}
