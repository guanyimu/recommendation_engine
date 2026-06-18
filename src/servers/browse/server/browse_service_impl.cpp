#include "browse_service_impl.h"

#include "logger.h"

namespace recommendation {
namespace {

UserId ParseUserId(const std::string &text) {
  UserId user_id = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return 0;
    }
    user_id = user_id * 10 + static_cast<UserId>(c - '0');
  }
  return user_id;
}

}  // namespace

grpc::Status BrowseServiceImpl::Browse(grpc::ServerContext *context,
                                       const BrowseRequest *request,
                                       BrowseReply *reply) {
  const UserId user_id = ParseUserId(request->userid());
  if (user_id == 0 && request->userid() != "0") {
    LOG(WARNING) << "[browse] 无效 user_id: " << request->userid();
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid user_id");
  }

  const std::vector<Video> videos = engine_.Browse(user_id);
  if (videos.empty()) {
    LOG(WARNING) << "[browse] Browse 返回空列表 user_id=" << user_id
                 << " peer=" << context->peer();
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "browse unavailable or misconfigured");
  }

  for (const Video &video : videos) {
    reply->add_video_list(video);
  }

  LOG(INFO) << "[browse] Browse RPC user_id=" << user_id
            << " videos=" << videos.size() << " peer=" << context->peer();
  return grpc::Status::OK;
}

}  // namespace recommendation
