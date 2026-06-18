#pragma once

#include "types.h"

#include <memory>
#include <string>
#include <vector>

namespace recommendation {

// Ranking gRPC 客户端（对外唯一入口）
class RankingClient {
 public:
  RankingClient();
  ~RankingClient();

  RankingClient(const RankingClient &) = delete;
  RankingClient &operator=(const RankingClient &) = delete;
  RankingClient(RankingClient &&) noexcept;
  RankingClient &operator=(RankingClient &&) noexcept;

  std::vector<ItemId> Rank(UserId user_id);
  const std::string &target() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace recommendation
