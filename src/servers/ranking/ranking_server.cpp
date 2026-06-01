#include "ranking_server.h"

#include <chrono>
#include <random>

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

RankingServer::RankingServer(const ConfigReader& config) : config_(config) {}

std::vector<ItemId> RankingServer::Rank(UserId user_id) const {
  (void)user_id;

  const std::size_t recommend_count =
      config_.GetSizeT("recommend_count", 8);

  std::mt19937 rng = MakeRng();
  std::uniform_int_distribution<ItemId> dist(kItemIdMin, kItemIdMax);

  std::vector<ItemId> item_ids;
  item_ids.reserve(recommend_count);
  for (std::size_t i = 0; i < recommend_count; ++i) {
    item_ids.push_back(dist(rng));
  }
  return item_ids;
}

}  // namespace recommendation
