#pragma once

#include "ranking_engine.h"

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/RankingService.grpc.pb.h"

namespace recommendation {

class RankingServiceImpl final : public RankingService::Service {
 public:
  RankingServiceImpl() = default;

  grpc::Status Ranking(grpc::ServerContext *context,
                       const RankingRequest *request,
                       RankingReply *reply) override;

 private:
  RankingEngine engine_;
};

}  // namespace recommendation
