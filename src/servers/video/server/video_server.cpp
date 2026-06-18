#include "video_server.h"

#include "config.h"
#include "logger.h"

namespace recommendation {

VideoServer::VideoServer() {
  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[video] 配置未加载";
    return;
  }
  if (!snap->RequireString("video_address", address_)) {
    LOG(ERROR) << "[video] 构造失败，监听地址未配置";
  }
}

VideoServer::~VideoServer() { Shutdown(); }

bool VideoServer::StartListening() {
  std::lock_guard<std::mutex> lock(mu_);
  if (shutdown_requested_) {
    return false;
  }

  grpc::ServerBuilder builder;
  builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);

  int bound_port = 0;
  builder.AddListeningPort(address_, grpc::InsecureServerCredentials(),
                           &bound_port);
  builder.RegisterService(&service_);
  server_ = builder.BuildAndStart();
  if (!server_ || bound_port == 0) {
    LOG(ERROR) << "[video] 无法监听地址（端口可能被占用）: " << address_;
    server_.reset();
    return false;
  }

  LOG(INFO) << "[video] gRPC 已绑定 " << address_ << " 端口 " << bound_port;
  return true;
}

void VideoServer::StopListeningLocked() { server_.reset(); }

bool VideoServer::Run() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    shutdown_requested_ = false;
  }

  while (true) {
    if (!StartListening()) {
      return false;
    }

    grpc::Server *server_ptr = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      server_ptr = server_.get();
    }

    server_ptr->Wait();

    bool should_exit = false;
    bool should_restart = false;
    {
      std::lock_guard<std::mutex> lock(mu_);
      StopListeningLocked();
      should_exit = shutdown_requested_;
      should_restart = restart_for_reload_;
      restart_for_reload_ = false;
    }

    if (should_exit) {
      break;
    }
    if (should_restart) {
      continue;
    }
    break;
  }

  return true;
}

void VideoServer::Shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  shutdown_requested_ = true;
  if (server_) {
    server_->Shutdown();
  }
}

bool VideoServer::Reload(const std::string &config_path) {
  if (!Config::Reload(config_path)) {
    return false;
  }

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[video] Reload 后配置快照为空";
    return false;
  }

  std::string new_address;
  if (!snap->RequireString("video_address", new_address)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mu_);
  if (new_address == address_) {
    LOG(INFO) << "[video] Reload 完成，监听地址未变 address=" << address_;
    return true;
  }

  LOG(INFO) << "[video] Reload 变更监听地址 " << address_ << " -> "
            << new_address;
  address_ = std::move(new_address);
  restart_for_reload_ = true;
  if (server_) {
    server_->Shutdown();
  }
  return true;
}

}  // namespace recommendation
