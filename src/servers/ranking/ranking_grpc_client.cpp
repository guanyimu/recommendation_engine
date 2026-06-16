#include "ranking_grpc_client.h"

#include "gated_config_reloader.h"
#include "logger.h"

#include <chrono>

namespace recommendation {
namespace {

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

} // namespace

bool RankingGrpcClient::ConnectLocked() {
  std::string config_target;
  if (!GatedConfig::Instance().RequireString("ranking_address",
                                             config_target)) {
    LOG(ERROR) << "[ranking_client] conf 缺少 ranking_address 或值为空";
    return false;
  }

  if (channel_ && target_ == config_target) {
    return true;
  }

  const std::string previous_target = target_;
  const bool had_channel = static_cast<bool>(channel_);

  if (previous_target.empty()) {
    LOG(INFO) << "[ranking_client] 首次连接 target=" << config_target;
  } else if (previous_target != config_target) {
    LOG(INFO) << "[ranking_client] ranking_address 变更，重连 "
              << previous_target << " -> " << config_target;
  } else if (!had_channel) {
    LOG(INFO) << "[ranking_client] 上次连接未就绪，同地址重试 target="
              << config_target;
  } else {
    LOG(INFO) << "[ranking_client] 重建 channel target=" << config_target;
  }

  target_ = config_target;
  channel_ = grpc::CreateChannel(target_, grpc::InsecureChannelCredentials());
  stub_ = RankingService::NewStub(channel_);
  return true;
}

RankingGrpcClient::RankingGrpcClient() {
  std::lock_guard<std::mutex> lock(mu_);
  if (!ConnectLocked()) {
    LOG(ERROR) << "[ranking_client] 构造失败，客户端未就绪";
  }
}

std::vector<ItemId> RankingGrpcClient::Rank(UserId user_id) {
  const auto rpc_start = std::chrono::steady_clock::now();

  int connect_timeout_sec = 0;
  if (!GatedConfig::Instance().RequireInt("ranking_connect_timeout_sec",
                                          connect_timeout_sec) ||
      connect_timeout_sec <= 0) {
    LOG(ERROR)
        << "[ranking_client] conf 缺少 ranking_connect_timeout_sec 或值非法";
    return {};
  }

  // TODO：这一段加锁感觉有些问题，到时候再看看
  std::shared_ptr<grpc::Channel> channel;
  RankingService::Stub *stub = nullptr;
  std::string target;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!ConnectLocked()) {
      return {};
    }
    target = target_;
    channel = channel_;
    stub = stub_.get();
  }

  const gpr_timespec deadline =
      gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
                   gpr_time_from_seconds(connect_timeout_sec, GPR_TIMESPAN));
  if (!channel->WaitForConnected(deadline)) {
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - rpc_start);
    LOG(ERROR) << "[ranking_client] 连接失败 target=" << target
               << " user_id=" << user_id << " 耗时 " << elapsed_ms.count()
               << " ms";
    std::lock_guard<std::mutex> lock(mu_);
    channel_.reset();
    stub_.reset();
    return {};
  }

  RankingRequest request;
  request.set_userid(std::to_string(user_id));

  RankingReply reply;
  grpc::ClientContext context;
  grpc::Status status = stub->Ranking(&context, request, &reply);

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - rpc_start);

  if (!status.ok()) {
    LOG(ERROR) << "[ranking_client] RPC 失败 target=" << target
               << " user_id=" << user_id << " code=" << status.error_code()
               << " msg=" << status.error_message() << " RPC耗时 "
               << elapsed_ms.count() << " ms";
    return {};
  }

  std::vector<ItemId> item_ids;
  item_ids.reserve(reply.itemid_list_size());
  for (const std::string &item_id_text : reply.itemid_list()) {
    item_ids.push_back(ParseItemId(item_id_text));
  }

  LOG(INFO) << "[ranking_client] RPC 成功 target=" << target
            << " user_id=" << user_id << " items=" << item_ids.size()
            << " RPC耗时 " << elapsed_ms.count() << " ms";
  return item_ids;
}

} // namespace recommendation
