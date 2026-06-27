#include "config.h"
#include "listen_util.h"
#include "logger.h"
#include "video_server.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace {

constexpr char kConfigPath[] = "configs/services.conf";

std::atomic<bool> g_reload_requested{false};
std::atomic<bool> g_shutdown{false};

void OnSigHup(int /*signo*/) { g_reload_requested.store(true); }

void OnSigInt(int /*signo*/) { g_shutdown.store(true); }

}  // namespace

int main(int argc, char **argv) {
  recommendation::LogOptions log_opts;
  recommendation::ApplyInstanceLogging(log_opts, "video",
                                       recommendation::kVideoLogDir);
  if (!recommendation::InitLogging(argc, argv, log_opts)) {
    return 1;
  }

  if (!recommendation::Config::Reload(kConfigPath)) {
    LOG(ERROR) << "[video] 无法加载配置: " << kConfigPath;
    recommendation::ShutdownLogging();
    return 1;
  }

  recommendation::VideoServer server;

  std::signal(SIGHUP, OnSigHup);
  std::signal(SIGINT, OnSigInt);

  std::thread control_thread([&server] {
    while (!g_shutdown.load()) {
      if (g_reload_requested.exchange(false)) {
        server.Reload(kConfigPath);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    server.Shutdown();
  });

  LOG(INFO) << "[video] 监听: " << server.address()
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
