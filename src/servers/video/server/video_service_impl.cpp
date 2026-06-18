#include "video_service_impl.h"

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

ItemId ParseItemId(const std::string &text) {
  ItemId value = 0;
  for (char c : text) {
    if (c < '0' || c > '9') {
      return 0;
    }
    value = value * 10 + static_cast<ItemId>(c - '0');
  }
  return value;
}

}  // namespace

grpc::Status VideoServiceImpl::Fetch(grpc::ServerContext *context,
                                     const FetchRequest *request,
                                     FetchReply *reply) {
  const UserId user_id = ParseUserId(request->userid());
  if (user_id == 0 && request->userid() != "0") {
    LOG(WARNING) << "[video] 无效 user_id: " << request->userid();
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid user_id");
  }

  std::vector<ItemId> item_ids;
  item_ids.reserve(request->itemid_list_size());
  for (const std::string &item_id_text : request->itemid_list()) {
    item_ids.push_back(ParseItemId(item_id_text));
  }

  const std::vector<Video> videos = engine_.Fetch(user_id, item_ids);
  if (videos.empty() && !item_ids.empty()) {
    LOG(WARNING) << "[video] Fetch 返回空列表 user_id=" << user_id
                 << " peer=" << context->peer();
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, "video unavailable");
  }

  for (const Video &video : videos) {
    reply->add_video_list(video);
  }

  LOG(INFO) << "[video] Fetch user_id=" << user_id << " videos=" << videos.size()
            << " peer=" << context->peer();
  return grpc::Status::OK;
}

}  // namespace recommendation
