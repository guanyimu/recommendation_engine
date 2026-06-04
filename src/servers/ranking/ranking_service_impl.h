#pragma once

#include "gated_config_reloader.h"
#include "ranking_server.h"

#include <grpcpp/grpcpp.h>
#include <mutex>

#include "proto/recommendation/RankingService.grpc.pb.h"

namespace recommendation {

// gRPC 服务端：把 RankingServer 逻辑暴露为 RankingService
class RankingServiceImpl final : public RankingService::Service {
 public:
  explicit RankingServiceImpl(GatedConfig& config);

  grpc::Status Ranking(grpc::ServerContext* context,
                       const RankingRequest* request,
                       RankingReply* reply) override;

 private:
  GatedConfig& config_;
  RankingServer ranking_server_;
  std::mutex log_mutex_;
};

}  // namespace recommendation
