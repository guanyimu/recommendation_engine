#pragma once

#include "types.h"

#include <vector>

namespace recommendation {

class VideoEngine {
 public:
  VideoEngine() = default;

  std::vector<Video> Fetch(UserId user_id,
                           const std::vector<ItemId> &item_ids) const;
};

}  // namespace recommendation
