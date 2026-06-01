#pragma once

#include "types.h"

#include <vector>

namespace recommendation {

// 拉视频服务器：把 item_id 列表转换成视频内容列表
class VideoServer {
 public:
  std::vector<Video> Fetch(const UserId& user_id, const std::vector<ItemId>& item_ids) const;
};

}  // namespace recommendation
