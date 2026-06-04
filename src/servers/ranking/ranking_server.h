#pragma once

#include "gated_config_reloader.h"
#include "types.h"

#include <vector>

namespace recommendation {

// 排序服务器：根据 user_id 返回推荐视频 item_id 列表
class RankingServer {
 public:
  explicit RankingServer(GatedConfig& config);

  std::vector<ItemId> Rank(UserId user_id) const;

 private:
  GatedConfig& config_;
};

}  // namespace recommendation
