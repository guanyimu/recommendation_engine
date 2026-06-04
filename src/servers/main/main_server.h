#pragma once

#include "types.h"
#include "video_server.h"

#include <vector>

namespace recommendation {

class RankingGrpcClient;

// 主服务器：通过 gRPC 调 Ranking，本地调 Video
class MainServer {
 public:
  MainServer(RankingGrpcClient& ranking_client, VideoServer& video_server);

  std::vector<Video> HandleBrowse(UserId user_id) const;

 private:
  RankingGrpcClient& ranking_client_;
  VideoServer& video_server_;
};

}  // namespace recommendation
