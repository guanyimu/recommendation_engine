#include "ranking_grpc_client.h"

#include <chrono>
#include <iostream>

namespace recommendation {
namespace {

ItemId ParseItemId(const std::string& text) {
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

RankingGrpcClient::RankingGrpcClient(std::string target)
    : target_(std::move(target)) {
  channel_ = grpc::CreateChannel(target_, grpc::InsecureChannelCredentials());
  stub_ = RankingService::NewStub(channel_);
}

std::vector<ItemId> RankingGrpcClient::Rank(UserId user_id) const {
  const auto rpc_start = std::chrono::steady_clock::now();

  const gpr_timespec deadline =
      gpr_time_add(gpr_now(GPR_CLOCK_REALTIME), gpr_time_from_seconds(5, GPR_TIMESPAN));
  if (!channel_->WaitForConnected(deadline)) {
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - rpc_start);
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cerr << "[ranking_client] 连接失败 target=" << target_
              << " user_id=" << user_id << " 耗时 " << elapsed_ms.count()
              << " ms\n";
    return {};
  }

  RankingRequest request;
  request.set_userid(std::to_string(user_id));

  RankingReply reply;
  grpc::ClientContext context;
  grpc::Status status = stub_->Ranking(&context, request, &reply);

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - rpc_start);

  if (!status.ok()) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cerr << "[ranking_client] RPC 失败 target=" << target_
              << " user_id=" << user_id << " code=" << status.error_code()
              << " msg=" << status.error_message() << " RPC耗时 "
              << elapsed_ms.count() << " ms\n";
    return {};
  }

  std::vector<ItemId> item_ids;
  item_ids.reserve(reply.itemid_list_size());
  for (const std::string& item_id_text : reply.itemid_list()) {
    item_ids.push_back(ParseItemId(item_id_text));
  }

  {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::cout << "[ranking_client] RPC 成功 target=" << target_
              << " user_id=" << user_id << " items=" << item_ids.size()
              << " RPC耗时 " << elapsed_ms.count() << " ms\n";
  }
  return item_ids;
}

}  // namespace recommendation
