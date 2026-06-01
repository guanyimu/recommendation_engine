#include "video_server.h"

#include <chrono>
#include <random>
#include <string>

namespace recommendation {
namespace {

// 占位随机后缀范围，接入数据库后删除
constexpr int kVideoSuffixMin = 1000;
constexpr int kVideoSuffixMax = 9999;

std::mt19937 MakeRng() {
  std::random_device rd;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::mt19937(static_cast<std::uint32_t>(now ^ rd()));
}

}  // namespace

std::vector<Video> VideoServer::Fetch(const UserId& user_id,
    const std::vector<ItemId>& item_ids) const {
  std::vector<Video> videos;
  videos.reserve(item_ids.size());

  std::mt19937 rng = MakeRng();
  std::uniform_int_distribution<int> suffix_dist(kVideoSuffixMin,
                                                 kVideoSuffixMax);

  for (ItemId item_id : item_ids) {
    videos.push_back("user_"+ std::to_string(user_id) +"_video_item_" + std::to_string(item_id) + "_clip_" +
                     std::to_string(suffix_dist(rng)));
  }
  return videos;
}

}  // namespace recommendation
