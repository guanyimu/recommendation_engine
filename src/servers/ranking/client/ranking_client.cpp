#include "ranking_client.h"

#include "config.h"
#include "logger.h"
#include "server_registration.h"
#include "service_names.h"

#include <chrono>
#include <mutex>

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/RankingService.grpc.pb.h"

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

}  // namespace

struct RankingClient::Impl {
  mutable std::mutex mu;
  std::string target;
  std::shared_ptr<grpc::Channel> channel;
  std::shared_ptr<RankingService::Stub> stub;

  bool ConnectLocked(const std::string &config_target) {
    if (channel && target == config_target) {
      return true;
    }

    const std::string previous_target = target;
    const bool had_channel = static_cast<bool>(channel);

    if (previous_target.empty()) {
      LOG(INFO) << "[ranking_client] 首次连接 target=" << config_target;
    } else if (previous_target != config_target) {
      LOG(INFO) << "[ranking_client] ranking 入口变更，重连 "
                << previous_target << " -> " << config_target;
    } else if (!had_channel) {
      LOG(INFO) << "[ranking_client] 上次连接未就绪，同地址重试 target="
                << config_target;
    } else {
      LOG(INFO) << "[ranking_client] 重建 channel target=" << config_target;
    }

    target = config_target;
    channel =
        grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    stub = std::shared_ptr<RankingService::Stub>(
        RankingService::NewStub(channel).release());
    return true;
  }
};

RankingClient::RankingClient() : impl_(std::make_unique<Impl>()) {
  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  std::string config_target;
  if (!snap || !ResolveServiceAddress(snap, kServiceRanking, &config_target)) {
    LOG(ERROR) << "[ranking_client] 构造失败，客户端未就绪";
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (!impl_->ConnectLocked(config_target)) {
    LOG(ERROR) << "[ranking_client] 构造失败，客户端未就绪";
  }
}

RankingClient::~RankingClient() = default;

RankingClient::RankingClient(RankingClient &&) noexcept = default;

RankingClient &RankingClient::operator=(RankingClient &&) noexcept = default;

const std::string &RankingClient::target() const {
  static const std::string kEmpty;
  if (!impl_) {
    return kEmpty;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->target;
}

std::vector<ItemId> RankingClient::Rank(UserId user_id) {
  const auto rpc_start = std::chrono::steady_clock::now();

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[ranking_client] 配置未加载";
    return {};
  }

  std::string config_target;
  if (!ResolveServiceAddress(snap, kServiceRanking, &config_target)) {
    return {};
  }

  int connect_timeout_sec = 0;
  if (!snap->RequirePositiveInt("ranking_connect_timeout_sec",
                                connect_timeout_sec)) {
    return {};
  }

  std::shared_ptr<grpc::Channel> channel;
  std::shared_ptr<RankingService::Stub> stub;
  std::string target;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (!impl_->ConnectLocked(config_target)) {
      return {};
    }
    target = impl_->target;
    channel = impl_->channel;
    stub = impl_->stub;
  }

  if (!channel || !stub) {
    LOG(ERROR) << "[ranking_client] channel/stub 未就绪 target=" << target;
    return {};
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
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (impl_->channel == channel) {
      impl_->channel.reset();
      impl_->stub.reset();
    }
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

}  // namespace recommendation
