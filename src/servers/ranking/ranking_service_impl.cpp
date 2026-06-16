#include "ranking_service_impl.h"

#include "logger.h"

#include <chrono>

namespace recommendation {

grpc::Status RankingServiceImpl::Ranking(grpc::ServerContext *context,
                                         const RankingRequest *request,
                                         RankingReply *reply) {
  const auto rpc_start = std::chrono::steady_clock::now();

  UserId user_id = 0;
  for (char c : request->userid()) {
    if (c < '0' || c > '9') {
      LOG(WARNING) << "[ranking] 无效 user_id: " << request->userid();
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid user_id");
    }
    user_id = user_id * 10 + static_cast<UserId>(c - '0');
  }

  const std::vector<ItemId> item_ids = ranking_server_.Rank(user_id);
  if (item_ids.empty()) {
    LOG(WARNING) << "[ranking] Ranking 返回空列表 user_id=" << user_id
                 << " peer=" << context->peer();
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "ranking unavailable or misconfigured");
  }

  for (ItemId item_id : item_ids) {
    reply->add_itemid_list(std::to_string(item_id));
  }

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - rpc_start);
  LOG(INFO) << "[ranking] Ranking user_id=" << user_id
            << " items=" << item_ids.size() << " 服务端处理耗时 "
            << elapsed_ms.count() << " ms peer=" << context->peer();
  return grpc::Status::OK;
}

} // namespace recommendation
