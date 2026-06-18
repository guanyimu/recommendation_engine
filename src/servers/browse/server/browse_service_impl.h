#pragma once

#include "browse_engine.h"

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/BrowseService.grpc.pb.h"

namespace recommendation {

class BrowseServiceImpl final : public BrowseService::Service {
 public:
  BrowseServiceImpl() = default;

  grpc::Status Browse(grpc::ServerContext *context, const BrowseRequest *request,
                      BrowseReply *reply) override;

 private:
  BrowseEngine engine_;
};

}  // namespace recommendation
