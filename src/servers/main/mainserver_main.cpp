#include "gated_config_reloader.h"
#include "logger.h"
#include "main_server.h"
#include "ranking_grpc_client.h"
#include "video_server.h"

#include "agent.h"
#include "types.h"

#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr char kConfigPath[] = "configs/ranking.conf";
constexpr int kAgentCount = 1000;

recommendation::UserId RandomUserId() {
  std::random_device rd;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::mt19937 rng(static_cast<std::uint32_t>(now ^ rd()));
  std::uniform_int_distribution<recommendation::UserId> dist(1, 1'000'000'000);
  return dist(rng);
}

}  // namespace

int main(int argc, char** argv) {
  recommendation::LogOptions log_opts;
  log_opts.server_name = "mainserver";
  log_opts.log_dir = recommendation::kMainServerLogDir;
  if (!recommendation::InitLogging(argc, argv, log_opts)) {
    return 1;
  }

  if (!recommendation::GatedConfig::Instance().Init(kConfigPath)) {
    LOG(ERROR) << "[mainserver] 无法加载配置: " << kConfigPath;
    recommendation::ShutdownLogging();
    return 1;
  }

  recommendation::GatedConfig& config = recommendation::GatedConfig::Instance();

  recommendation::RankingGrpcClient ranking_client;
  recommendation::VideoServer video_server;
  recommendation::MainServer main_server(ranking_client, video_server);

  std::string ranking_address;
  if (!config.RequireString("ranking_address", ranking_address)) {
    LOG(ERROR) << "[mainserver] conf 缺少 ranking_address 或值为空";
    recommendation::ShutdownLogging();
    return 1;
  }

  int recommend_count = 0;
  if (!config.RequireInt("recommend_count", recommend_count)) {
    LOG(ERROR) << "[mainserver] conf 缺少 recommend_count 或值非法";
    recommendation::ShutdownLogging();
    return 1;
  }

  LOG(INFO) << "[mainserver] 请先另开终端启动 Ranking: ./tools/run_ranking.sh";
  LOG(INFO) << "[mainserver] Ranking gRPC 地址: " << ranking_address;
  LOG(INFO) << "[mainserver] recommend_count=" << recommend_count;
  LOG(INFO) << "[mainserver] 启动 " << kAgentCount << " 个 Agent 并发刷视频...";

  std::vector<std::thread> agents;
  agents.reserve(kAgentCount);

  const auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < kAgentCount; ++i) {
    agents.emplace_back([&, i] {
      recommendation::Agent agent(RandomUserId());
      const auto videos = agent.BrowseVideos(main_server);

      LOG(INFO) << "[mainserver] Agent #" << i << " user_id=" << agent.user_id()
                << " 收到 " << videos.size() << " 个视频"
                << "（期望 " << recommend_count << "）";
    });
  }

  for (std::thread& t : agents) {
    t.join();
  }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);
  LOG(INFO) << "[mainserver] " << kAgentCount << " 个并发请求总耗时 "
            << elapsed.count() << " ms";

  recommendation::ShutdownLogging();
  return 0;
}
