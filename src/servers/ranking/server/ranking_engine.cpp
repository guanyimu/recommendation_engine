#include "ranking_engine.h"

#include "config.h"
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

}  // namespace

std::vector<ItemId> RankingEngine::Rank(UserId user_id) const {
  (void)user_id;

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[ranking] 配置未加载";
    return {};
  }

  int recommend_count = 0;
  if (!snap->RequireNonNegativeInt("recommend_count", recommend_count)) {
    return {};
  }

  std::mt19937 rng = MakeRng();

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

}  // namespace recommendation
