#include "browse_engine.h"

#include "logger.h"
#include "ranking_client.h"
#include "video_client.h"

namespace recommendation {

BrowseEngine::BrowseEngine()
    : ranking_client_(std::make_unique<RankingClient>()),
      video_client_(std::make_unique<VideoClient>()) {}

BrowseEngine::~BrowseEngine() = default;

std::vector<Video> BrowseEngine::Browse(UserId user_id) {
  LOG(INFO) << "[browse] Browse user_id=" << user_id;
  const std::vector<ItemId> item_ids = ranking_client_->Rank(user_id);
  if (item_ids.empty()) {
    LOG(WARNING) << "[browse] Ranking 返回空列表 user_id=" << user_id;
    return {};
  }
  std::vector<Video> videos = video_client_->Fetch(user_id, item_ids);
  LOG(INFO) << "[browse] Browse 完成 user_id=" << user_id
            << " videos=" << videos.size();
  return videos;
}

}  // namespace recommendation
