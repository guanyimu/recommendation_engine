#include "video_client.h"

#include "config.h"
#include "logger.h"
#include "server_registration.h"
#include "service_names.h"

#include <chrono>
#include <mutex>

#include <grpcpp/grpcpp.h>

#include "proto/recommendation/VideoService.grpc.pb.h"

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

struct VideoClient::Impl {
  mutable std::mutex mu;
  std::string target;
  std::shared_ptr<grpc::Channel> channel;
  std::shared_ptr<VideoService::Stub> stub;

  bool ConnectLocked(const std::string &config_target) {
    if (channel && target == config_target) {
      return true;
    }

    const std::string previous_target = target;
    if (previous_target.empty()) {
      LOG(INFO) << "[video_client] 首次连接 target=" << config_target;
    } else if (previous_target != config_target) {
      LOG(INFO) << "[video_client] video 入口变更，重连 "
                << previous_target << " -> " << config_target;
    }

    target = config_target;
    channel =
        grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    stub = std::shared_ptr<VideoService::Stub>(
        VideoService::NewStub(channel).release());
    return true;
  }
};

VideoClient::VideoClient() : impl_(std::make_unique<Impl>()) {
  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  std::string config_target;
  if (!snap || !ResolveServiceAddress(snap, kServiceVideo, &config_target)) {
    LOG(ERROR) << "[video_client] 构造失败，客户端未就绪";
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (!impl_->ConnectLocked(config_target)) {
    LOG(ERROR) << "[video_client] 构造失败，客户端未就绪";
  }
}

VideoClient::~VideoClient() = default;

VideoClient::VideoClient(VideoClient &&) noexcept = default;

VideoClient &VideoClient::operator=(VideoClient &&) noexcept = default;

const std::string &VideoClient::target() const {
  static const std::string kEmpty;
  if (!impl_) {
    return kEmpty;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->target;
}

std::vector<Video> VideoClient::Fetch(UserId user_id,
                                      const std::vector<ItemId> &item_ids) {
  if (item_ids.empty()) {
    return {};
  }

  const auto rpc_start = std::chrono::steady_clock::now();

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[video_client] 配置未加载";
    return {};
  }

  std::string config_target;
  if (!ResolveServiceAddress(snap, kServiceVideo, &config_target)) {
    return {};
  }

  int connect_timeout_sec = 0;
  if (!snap->RequirePositiveInt("video_connect_timeout_sec",
                                connect_timeout_sec)) {
    return {};
  }

  std::shared_ptr<grpc::Channel> channel;
  std::shared_ptr<VideoService::Stub> stub;
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
    LOG(ERROR) << "[video_client] channel/stub 未就绪 target=" << target;
    return {};
  }

  const gpr_timespec deadline =
      gpr_time_add(gpr_now(GPR_CLOCK_REALTIME),
                   gpr_time_from_seconds(connect_timeout_sec, GPR_TIMESPAN));
  if (!channel->WaitForConnected(deadline)) {
    LOG(ERROR) << "[video_client] 连接失败 target=" << target
               << " user_id=" << user_id;
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (impl_->channel == channel) {
      impl_->channel.reset();
      impl_->stub.reset();
    }
    return {};
  }

  FetchRequest request;
  request.set_userid(std::to_string(user_id));
  for (ItemId item_id : item_ids) {
    request.add_itemid_list(std::to_string(item_id));
  }

  FetchReply reply;
  grpc::ClientContext context;
  grpc::Status status = stub->Fetch(&context, request, &reply);

  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - rpc_start);

  if (!status.ok()) {
    LOG(ERROR) << "[video_client] RPC 失败 target=" << target
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

  LOG(INFO) << "[video_client] RPC 成功 target=" << target
            << " user_id=" << user_id << " videos=" << videos.size()
            << " RPC耗时 " << elapsed_ms.count() << " ms";
  return videos;
}

}  // namespace recommendation
