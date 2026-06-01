#include "main_server.h"

namespace recommendation {

MainServer::MainServer(RankingServer& ranking_server,
                         VideoServer& video_server)
    : ranking_server_(ranking_server), video_server_(video_server) {}

std::vector<Video> MainServer::HandleBrowse(UserId user_id) const {
  std::vector<ItemId> item_ids = ranking_server_.Rank(user_id);
  return video_server_.Fetch(user_id, item_ids);
}

}  // namespace recommendation
