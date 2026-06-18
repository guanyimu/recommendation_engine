#pragma once

#include "types.h"

#include <vector>

namespace recommendation {

// 排序业务逻辑（包内私有）
class RankingEngine {
 public:
  RankingEngine() = default;

  std::vector<ItemId> Rank(UserId user_id) const;
};

}  // namespace recommendation
