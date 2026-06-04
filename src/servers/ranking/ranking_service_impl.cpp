#include "ranking_service_impl.h"

#include <chrono>
#include <iostream>

namespace recommendation {

RankingServiceImpl::RankingServiceImpl(GatedConfig& config)
    : config_(config), ranking_server_(config) {}

grpc::Status RankingServiceImpl::Ranking(grpc::ServerContext* context,
                                          const RankingRequest* request,
                                          RankingReply* reply) {
  const auto rpc_start = std::chrono::steady_clock::now();

  GatedConfig::RequestGuard request_guard(config_);
  if (!request_guard.ok()) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cerr << "[ranking] 拒绝请求：服务暂不可用（Reload 中？） peer="
              << context->peer() << '\n';
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "service not accepting requests");
  }

  UserId user_id = 0;
  for (char c : request->userid()) {
    if (c < '0' || c > '9') {
      std::lock_guard<std::mutex> lock(log_mutex_);
      std::cerr << "[ranking] 无效 user_id: " << request->userid() << '\n';
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid user_id");
    }
    user_id = user_id * 10 + static_cast<UserId>(c - '0');
  }

  const std::vector<ItemId> item_ids = ranking_server_.Rank(user_id);
  for (ItemId item_id : item_ids) {
    reply->add_itemid_list(std::to_string(item_id));
  }

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - rpc_start);
  {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << "[ranking] Ranking user_id=" << user_id << " items="
              << item_ids.size() << " 服务端处理耗时 " << elapsed_ms.count()
              << " ms peer=" << context->peer() << '\n';
  }
  return grpc::Status::OK;
}

}  // namespace recommendation
