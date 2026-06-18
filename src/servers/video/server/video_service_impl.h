#pragma once

#include "video_engine.h"

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/VideoService.grpc.pb.h"

namespace recommendation {

class VideoServiceImpl final : public VideoService::Service {
 public:
  VideoServiceImpl() = default;

  grpc::Status Fetch(grpc::ServerContext *context, const FetchRequest *request,
                     FetchReply *reply) override;

 private:
  VideoEngine engine_;
};

}  // namespace recommendation
