#pragma once

#include "types.h"

#include <vector>

namespace recommendation {

class RankingServer {
public:
  RankingServer() = default;

  std::vector<ItemId> Rank(UserId user_id) const;
};

} // namespace recommendation
