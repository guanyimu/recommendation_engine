#pragma once

#include "types.h"
#include "ranking_server.h"
#include "video_server.h"

#include <vector>

namespace recommendation {

// 主服务器：协调排序服务器和拉视频服务器
class MainServer {
 public:
  MainServer(RankingServer& ranking_server, VideoServer& video_server);

  std::vector<Video> HandleBrowse(UserId user_id) const;

 private:
  RankingServer& ranking_server_;
  VideoServer& video_server_;
};

}  // namespace recommendation
