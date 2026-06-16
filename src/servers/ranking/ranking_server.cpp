#include "ranking_server.h"

#include "gated_config_reloader.h"
#include "logger.h"

#include <chrono>
#include <random>
#include <thread>

namespace recommendation {
namespace {

constexpr ItemId kItemIdMin = 1;
constexpr ItemId kItemIdMax = 1'000'000;

std::mt19937 MakeRng() {
  std::random_device rd;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::mt19937(static_cast<std::uint32_t>(now ^ rd()));
}

} // namespace

std::vector<ItemId> RankingServer::Rank(UserId user_id) const {
  (void)user_id;

  int recommend_count = 0;
  if (!GatedConfig::Instance().RequireInt("recommend_count", recommend_count) ||
      recommend_count < 0) {
    LOG(ERROR) << "[ranking] conf 缺少 recommend_count 或值非法";
    return {};
  }

  std::mt19937 rng = MakeRng();

  // 模拟排序服务耗时（1～2 秒）
  std::uniform_int_distribution<int> delay_ms(1000, 2000);
  std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms(rng)));
  std::uniform_int_distribution<ItemId> dist(kItemIdMin, kItemIdMax);

  std::vector<ItemId> item_ids;
  item_ids.reserve(static_cast<std::size_t>(recommend_count));
  for (int i = 0; i < recommend_count; ++i) {
    item_ids.push_back(dist(rng));
  }
  return item_ids;
}

} // namespace recommendation
