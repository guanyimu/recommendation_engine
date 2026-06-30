#include "config.h"
#include "logger.h"
#include "registry_http_server.h"
#include "registry_store.h"
#include "service_names.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace {

constexpr char kConfigPath[] = "configs/services.conf";

std::atomic<bool> g_shutdown{false};

void OnSignal(int /*signo*/) { g_shutdown.store(true); }

}  // namespace

int main(int argc, char **argv) {
  recommendation::LogOptions log_opts;
  log_opts.server_name = "registry";
  log_opts.log_dir = "logs/registry";
  if (!recommendation::InitLogging(argc, argv, log_opts)) {
    return 1;
  }

  if (!recommendation::Config::Reload(kConfigPath)) {
    LOG(ERROR) << "[registry] 无法加载配置: " << kConfigPath;
    recommendation::ShutdownLogging();
    return 1;
  }

  const std::shared_ptr<const recommendation::ConfigSnapshot> snap =
      recommendation::Config::GetSnapshot();
  std::string listen_address;
  if (!snap || !snap->RequireString(recommendation::kRegistryAddressKey,
                                    listen_address)) {
    LOG(ERROR) << "[registry] 配置缺少 " << recommendation::kRegistryAddressKey;
    recommendation::ShutdownLogging();
    return 1;
  }

  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);

  recommendation::registry::RegistryStore store;
  recommendation::registry::RegistryHttpServer server(&store);

  std::atomic<bool> ready{false};
  std::thread serving(
      [&] { server.Run(listen_address, &ready); });

  for (int i = 0; i < 50 && !ready.load(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if (!ready.load()) {
    LOG(ERROR) << "[registry] 名字服务启动失败（bind/listen 未成功）";
    recommendation::ShutdownLogging();
    return 1;
  }

  LOG(INFO) << "[registry] 名字服务就绪 " << listen_address;

  while (!g_shutdown.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  LOG(INFO) << "[registry] 正在退出";
  if (serving.joinable()) {
    serving.detach();
  }

  recommendation::ShutdownLogging();
  return 0;
}
