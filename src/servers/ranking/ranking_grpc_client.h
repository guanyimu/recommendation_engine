#pragma once

#include "types.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/RankingService.grpc.pb.h"

namespace recommendation {

// gRPC 客户端：构造时从 GatedConfig 读 ranking_address 建连；
// 每次 Rank 再读 conf，仅当 target 变化或需重试时重建 channel。
class RankingGrpcClient {
public:
  RankingGrpcClient();

  std::vector<ItemId> Rank(UserId user_id);

  const std::string &target() const { return target_; }

private:
  // 调用方已持有 mu_。从 GatedConfig 读 ranking_address，按需建连/重连。
  bool ConnectLocked();

  mutable std::mutex mu_;
  std::string target_{""};
  std::shared_ptr<grpc::Channel> channel_{nullptr};
  std::unique_ptr<RankingService::Stub> stub_{nullptr};
};

} // namespace recommendation
