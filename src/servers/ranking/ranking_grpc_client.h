#pragma once

#include "types.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/RankingService.grpc.pb.h"

namespace recommendation {

// gRPC 客户端：Main 用来调用 Ranking 服务
class RankingGrpcClient {
 public:
  explicit RankingGrpcClient(std::string target);

  std::vector<ItemId> Rank(UserId user_id) const;

  const std::string& target() const { return target_; }

 private:
  std::string target_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<RankingService::Stub> stub_;
  mutable std::mutex log_mutex_;
};

}  // namespace recommendation
