#include "main_server.h"

#include "ranking_grpc_client.h"

namespace recommendation {

MainServer::MainServer(RankingGrpcClient& ranking_client,
                       VideoServer& video_server)
    : ranking_client_(ranking_client), video_server_(video_server) {}

std::vector<Video> MainServer::HandleBrowse(UserId user_id) const {
  std::vector<ItemId> item_ids = ranking_client_.Rank(user_id);
  return video_server_.Fetch(user_id, item_ids);
}

}  // namespace recommendation
