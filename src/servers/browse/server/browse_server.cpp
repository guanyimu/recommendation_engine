#include "browse_server.h"

#include "config.h"
#include "logger.h"

namespace recommendation {

BrowseServer::BrowseServer() {
  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[browse] 配置未加载";
    return;
  }
  if (!snap->RequireString("browse_address", address_)) {
    LOG(ERROR) << "[browse] 构造失败，监听地址未配置";
  }
}

BrowseServer::~BrowseServer() { Shutdown(); }

bool BrowseServer::StartListening() {
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
    LOG(ERROR) << "[browse] 无法监听地址（端口可能被占用）: " << address_;
    server_.reset();
    return false;
  }

  LOG(INFO) << "[browse] gRPC 已绑定 " << address_ << " 端口 " << bound_port;
  return true;
}

void BrowseServer::StopListeningLocked() { server_.reset(); }

bool BrowseServer::Run() {
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

void BrowseServer::Shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  shutdown_requested_ = true;
  if (server_) {
    server_->Shutdown();
  }
}

bool BrowseServer::Reload(const std::string &config_path) {
  if (!Config::Reload(config_path)) {
    return false;
  }

  const std::shared_ptr<const ConfigSnapshot> snap = Config::GetSnapshot();
  if (!snap) {
    LOG(ERROR) << "[browse] Reload 后配置快照为空";
    return false;
  }

  std::string new_address;
  if (!snap->RequireString("browse_address", new_address)) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mu_);
  if (new_address == address_) {
    LOG(INFO) << "[browse] Reload 完成，监听地址未变 address=" << address_;
    return true;
  }

  LOG(INFO) << "[browse] Reload 变更监听地址 " << address_ << " -> "
            << new_address;
  address_ = std::move(new_address);
  restart_for_reload_ = true;
  if (server_) {
    server_->Shutdown();
  }
  return true;
}

}  // namespace recommendation
