#include "agent.h"
#include "config.h"
#include "main_server.h"
#include "ranking_server.h"
#include "video_server.h"

#include "types.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace {

constexpr char config_path[] = "configs/ranking.conf";

bool WriteRecommendCount(const char* path, std::size_t count) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << "# ranking 服务配置（demo 自动生成）\n";
  out << "recommend_count=" << count << '\n';
  return static_cast<bool>(out);
}

recommendation::UserId RandomUserId() {
  std::random_device rd;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::mt19937 rng(static_cast<std::uint32_t>(now ^ rd()));
  std::uniform_int_distribution<recommendation::UserId> dist(1, 1'000'000'000);
  return dist(rng);
}

void RunBrowse(recommendation::Agent& agent,
               recommendation::MainServer& main_server,
               std::size_t expected_count) {
  const auto videos = agent.BrowseVideos(main_server);
  std::cout << "收到 " << videos.size() << " 个视频"
            << "（期望 " << expected_count << " 个）:\n";
  for (const auto& video : videos) {
    std::cout << "  - " << video << '\n';
  }
}

bool ReloadFromConfigCenter(recommendation::ServiceGate& gate,
                            recommendation::ConfigManager& config) {
  std::cout << "\n[配置中心] 下发 Reload 命令...\n";

  // TODO：这里的config.Reload()需要读取日志然后阻塞，之前的WaitDrained又在阻塞等待已有的请求结束，这俩阻塞会导致阻塞等待时间过长，需要优化
  // 比如说把Reload中的读文件解耦出来放在WaidDrained之前？                           
  gate.StopAccepting();
  gate.WaitDrained();
  const bool ok = config.Reload();
  gate.StartAccepting();

  if (ok) {
    std::cout << "[配置中心] Reload 完成，recommend_count="
              << config.GetSizeT("recommend_count", 8) << '\n';
  } else {
    std::cerr << "[配置中心] Reload 失败: " << config.path() << '\n';
  }
  return ok;
}

}  // namespace

int main() {
  if (!WriteRecommendCount(config_path, 8)) {
    std::cerr << "无法写入配置: " << config_path << '\n';
    return 1;
  }

  recommendation::ConfigManager& config =
      recommendation::ConfigManager::Instance();
  if (!config.Init(config_path)) {
    std::cerr << "无法加载配置: " << config_path << '\n';
    return 1;
  }

  const recommendation::UserId user_id = RandomUserId();

  recommendation::ServiceGate gate;
  recommendation::RankingServer ranking_server(config);
  recommendation::VideoServer video_server;
  recommendation::MainServer main_server(ranking_server, video_server);
  recommendation::Agent agent(user_id);

  std::cout << "配置文件: " << config_path << '\n';
  std::cout << "初始 recommend_count="
            << config.GetSizeT("recommend_count", 8) << '\n';
  std::cout << "Agent user_id=" << agent.user_id() << "（随机生成）发起刷视频...\n";

  // TODO: 这个是不是应该整体写在RunBrowse里面，把ServiceGate变成单例的，这样就算我开多个进程每个都有一个agent也还正好可以？
  {
    recommendation::ServiceGate::Guard request_guard(gate);
    if (!request_guard.ok()) {
      std::cerr << "服务暂不可用\n";
      return 1;
    }
    RunBrowse(agent, main_server, 8);
  }

  if (!WriteRecommendCount(config_path, 10)) {
    std::cerr << "无法更新配置: " << config_path << '\n';
    return 1;
  }
  std::cout << "\n已将配置文件 recommend_count 改为 10\n";

  ReloadFromConfigCenter(gate, config);

  std::cout << "\nReload 后再次刷视频...\n";
  {
    recommendation::ServiceGate::Guard request_guard(gate);
    if (!request_guard.ok()) {
      std::cerr << "服务暂不可用\n";
      return 1;
    }
    RunBrowse(agent, main_server, 10);
  }

  return 0;
}
