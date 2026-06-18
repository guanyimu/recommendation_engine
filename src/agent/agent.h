#pragma once

#include "browse_client.h"
#include "types.h"

#include <vector>

namespace recommendation {

// 模拟一个用户终端：持有自己的 BrowseClient
class Agent {
 public:
  explicit Agent(UserId user_id);

  Agent(const Agent &) = delete;
  Agent &operator=(const Agent &) = delete;

  UserId user_id() const { return user_id_; }

  std::vector<Video> BrowseVideos();

 private:
  UserId user_id_;
  BrowseClient browse_client_;
};

}  // namespace recommendation
