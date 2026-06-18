#include "browse_client.h"

#include "config.h"
#include "logger.h"

#include <chrono>
#include <mutex>

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/BrowseService.grpc.pb.h"

namespace recommendation {

struct BrowseClient::Impl {
  mutable std::mutex mu;
  std::string target{""};
  std::shared_ptr<grpc::Channel> channel{nullptr};
  std::shared_ptr<BrowseService::Stub> stub{nullptr};

  bool ConnectLocked(const std::string &config_target) {
    if (channel && target == config_target) {
      return true;
    }

    const std::string previous_target = target;
    if (previous_target.empty()) {
      LOG(INFO) << "[browse_client] 首次连接 target=" << config_target;
    } else if (previous_target != config_target) {
      LOG(INFO) << "[browse_client] browse_address 变更，重连 "
                << previous_target << " -> " << config_target;
    }

    target = config_target;
    channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    stub = std::shared_ptr<BrowseService::Stub>(
        BrowseService::NewStub(channel).release());
    return true;
  }
};

BrowseClient::BrowseClient() : impl_(std::make_unique<Impl>()) {
  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  std::string config_target;
  if (!snap || !snap->RequireString("browse_address", config_target)) {
    LOG(ERROR) << "[browse_client] 构造失败，客户端未就绪";
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (!impl_->ConnectLocked(config_target)) {
    LOG(ERROR) << "[browse_client] 构造失败，客户端未就绪";
  }
}

BrowseClient::~BrowseClient() = default;

BrowseClient::BrowseClient(BrowseClient &&) noexcept = default;

BrowseClient &BrowseClient::operator=(BrowseClient &&) noexcept = default;

const std::string &BrowseClient::target() const {
  static const std::string kEmpty;
  if (!impl_) {
    return kEmpty;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->target;
}

std::vector<Video> BrowseClient::Browse(UserId user_id) {
  const auto rpc_start = std::chrono::steady_clock::now();

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[browse_client] 配置未加载";
    return {};
  }

  std::string config_target;
  if (!snap->RequireString("browse_address", config_target)) {
    return {};
  }

  int connect_timeout_sec = 0;
  if (!snap->RequirePositiveInt("browse_connect_timeout_sec",
                                connect_timeout_sec)) {
    return {};
  }

  std::shared_ptr<grpc::Channel> channel;
  std::shared_ptr<BrowseService::Stub> stub;
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
    LOG(ERROR) << "[browse_client] channel/stub 未就绪 target=" << target;
    return {};
  }

  const gpr_timespec deadline =
      gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
                   gpr_time_from_seconds(connect_timeout_sec, GPR_TIMESPAN));
  if (!channel->WaitForConnected(deadline)) {
    LOG(ERROR) << "[browse_client] 连接失败 target=" << target
               << " user_id=" << user_id;
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (impl_->channel == channel) {
      impl_->channel.reset();
      impl_->stub.reset();
    }
    return {};
  }

  BrowseRequest request;
  request.set_userid(std::to_string(user_id));

  BrowseReply reply;
  grpc::ClientContext context;
  grpc::Status status = stub->Browse(&context, request, &reply);

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - rpc_start);

  if (!status.ok()) {
    LOG(ERROR) << "[browse_client] RPC 失败 target=" << target
               << " user_id=" << user_id << " code=" << status.error_code()
               << " msg=" << status.error_message() << " RPC耗时 "
               << elapsed_ms.count() << " ms";
    return {};
  }

  std::vector<Video> videos;
  videos.reserve(reply.video_list_size());
  for (const std::string &video : reply.video_list()) {
    videos.push_back(video);
  }

  LOG(INFO) << "[browse_client] RPC 成功 target=" << target
            << " user_id=" << user_id << " videos=" << videos.size()
            << " RPC耗时 " << elapsed_ms.count() << " ms";
  return videos;
}

}  // namespace recommendation
