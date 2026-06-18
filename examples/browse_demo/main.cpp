#include "agent.h"
#include "config.h"
#include "logger.h"

#include "types.h"

#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr char kConfigPath[] = "configs/services.conf";
constexpr int kAgentCount = 1000;

recommendation::UserId RandomUserId() {
  std::random_device rd;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::mt19937 rng(static_cast<std::uint32_t>(now ^ rd()));
  std::uniform_int_distribution<recommendation::UserId> dist(1, 1'000'000'000);
  return dist(rng);
}

}  // namespace

int main(int argc, char **argv) {
  recommendation::LogOptions log_opts;
  log_opts.server_name = "browse_demo";
  log_opts.log_dir = recommendation::kBrowseLogDir;
  if (!recommendation::InitLogging(argc, argv, log_opts)) {
    return 1;
  }

  if (!recommendation::Config::Reload(kConfigPath)) {
    LOG(ERROR) << "[browse_demo] 无法加载配置: " << kConfigPath;
    recommendation::ShutdownLogging();
    return 1;
  }

  const std::shared_ptr<const recommendation::ConfigSnapshot> snap =
      recommendation::Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[browse_demo] 配置快照为空";
    recommendation::ShutdownLogging();
    return 1;
  }

  std::string browse_address;
  if (!snap->RequireString("browse_address", browse_address)) {
    recommendation::ShutdownLogging();
    return 1;
  }

  int recommend_count = 0;
  if (!snap->RequireNonNegativeInt("recommend_count", recommend_count)) {
    recommendation::ShutdownLogging();
    return 1;
  }

  LOG(INFO) << "[browse_demo] BrowseServer gRPC: " << browse_address;
  LOG(INFO) << "[browse_demo] recommend_count=" << recommend_count;
  LOG(INFO) << "[browse_demo] 启动 " << kAgentCount << " 个 Agent 并发刷视频...";

  std::vector<std::thread> agents;
  agents.reserve(kAgentCount);

  const auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < kAgentCount; ++i) {
    agents.emplace_back([&, i] {
      recommendation::Agent agent(RandomUserId());
      const auto videos = agent.BrowseVideos();

      LOG(INFO) << "[browse_demo] Agent #" << i << " user_id=" << agent.user_id()
                << " 收到 " << videos.size() << " 个视频"
                << "（期望 " << recommend_count << "）";
    });
  }

  for (std::thread &t : agents) {
    t.join();
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  LOG(INFO) << "[browse_demo] " << kAgentCount << " 个并发请求总耗时 "
            << elapsed.count() << " ms";

  recommendation::ShutdownLogging();
  return 0;
}
