#include "agent.h"
#include "gated_config_reloader.h"
#include "main_server.h"
#include "ranking_grpc_client.h"
#include "video_server.h"

#include "types.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr char config_path[] = "configs/ranking.conf";
constexpr int kAgentCount = 10;

recommendation::UserId RandomUserId() {
  std::random_device rd;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::mt19937 rng(static_cast<std::uint32_t>(now ^ rd()));
  std::uniform_int_distribution<recommendation::UserId> dist(1, 1'000'000'000);
  return dist(rng);
}

}  // namespace

int main() {

  if (!recommendation::GatedConfig::Init(config_path)) {
    std::cerr << "无法加载配置: " << config_path << '\n';
    return 1;
  }

  recommendation::GatedConfig config;

  std::string ranking_address;
  if (!config.RequireString("ranking_address", &ranking_address)) {
    std::cerr << "[browse_demo] conf 缺少 ranking_address 或值为空\n";
    return 1;
  }

  std::size_t recommend_count = 0;
  if (!config.RequireSizeT("recommend_count", &recommend_count)) {
    std::cerr << "[browse_demo] conf 缺少 recommend_count 或值非法\n";
    return 1;
  }

  recommendation::RankingGrpcClient ranking_client(ranking_address);
  recommendation::VideoServer video_server;
  recommendation::MainServer main_server(ranking_client, video_server);

  std::cout << "请先另开终端启动 Ranking 服务:\n";
  std::cout << "  bazel run //src/servers/ranking:ranking_main\n\n";
  std::cout << "Ranking gRPC 地址（来自 conf）: " << ranking_address << '\n';
  std::cout << "recommend_count=" << recommend_count << '\n';
  std::cout << "启动 " << kAgentCount << " 个 Agent 并发刷视频...\n";

  std::mutex log_mutex;
  std::vector<std::thread> agents;
  agents.reserve(kAgentCount);

  const auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < kAgentCount; ++i) {
    agents.emplace_back([&, i] {
      recommendation::Agent agent(RandomUserId());
      const auto videos = agent.BrowseVideos(main_server);

      std::lock_guard<std::mutex> lock(log_mutex);
      std::cout << "[browse_demo] Agent #" << i << " user_id=" << agent.user_id()
                << " 收到 " << videos.size() << " 个视频"
                << "（期望 " << recommend_count << "）\n";
    });
  }

  for (std::thread& t : agents) {
    t.join();
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << "\n[browse_demo] " << kAgentCount << " 个并发请求总耗时 "
            << elapsed.count() << " ms\n";
  std::cout << "（Ranking 每次随机 sleep 1～2s；若总耗时接近 1～2s 而非 "
            << kAgentCount << "s+，说明 gRPC 并发生效）\n";

  return 0;
}
