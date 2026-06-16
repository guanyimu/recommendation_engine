#include "main_server.h"

#include "logger.h"
#include "ranking_grpc_client.h"

namespace recommendation {

MainServer::MainServer(RankingGrpcClient& ranking_client,
                       VideoServer& video_server)
    : ranking_client_(ranking_client), video_server_(video_server) {}

std::vector<Video> MainServer::HandleBrowse(UserId user_id) const {
  LOG(INFO) << "[mainserver] HandleBrowse user_id=" << user_id;
  std::vector<ItemId> item_ids = ranking_client_.Rank(user_id);
  if (item_ids.empty()) {
    LOG(WARNING) << "[mainserver] Ranking 返回空列表 user_id=" << user_id;
    return {};
  }
  std::vector<Video> videos = video_server_.Fetch(user_id, item_ids);
  LOG(INFO) << "[mainserver] HandleBrowse 完成 user_id=" << user_id
            << " videos=" << videos.size();
  return videos;
}

}  // namespace recommendation
