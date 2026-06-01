#pragma once

#include "types.h"
#include "main_server.h"

#include <vector>

namespace recommendation {

// 智能体：模拟用户，持有 user_id，可发起刷视频请求
class Agent {
 public:
  explicit Agent(UserId user_id);

  UserId user_id() const { return user_id_; }

  // 刷视频：请求主服务器，返回视频列表
  std::vector<Video> BrowseVideos(const MainServer& main_server) const;

 private:
  UserId user_id_;
};

}  // namespace recommendation
