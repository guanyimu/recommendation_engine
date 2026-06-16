#pragma once

#include "gated_config_reloader.h"
#include "ranking_server.h"

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/RankingService.grpc.pb.h"

namespace recommendation {

// gRPC 服务端：把 RankingServer 逻辑暴露为 RankingService
class RankingServiceImpl final : public RankingService::Service {
 public:
  RankingServiceImpl() = default;

  grpc::Status Ranking(grpc::ServerContext* context,
                       const RankingRequest* request,
                       RankingReply* reply) override;

 private:
  RankingServer ranking_server_;
};

}  // namespace recommendation
