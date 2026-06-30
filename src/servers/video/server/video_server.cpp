#include "video_server.h"

#include "config.h"
#include "grpc_listen_util.h"
#include "logger.h"
#include "net_util.h"
#include "service_names.h"

namespace recommendation {

VideoServer::VideoServer()
    : registration_(kServiceVideo), address_(FormatHostPort(kLoopbackHost, 0)) {}

VideoServer::~VideoServer() { Shutdown(); }

bool VideoServer::StartListening() {
  std::lock_guard<std::mutex> lock(mu_);
  if (shutdown_requested_) {
    return false;
  }

  int bound_port = 0;
  if (!StartGrpcWithRetry(
          [&](grpc::ServerBuilder &builder) {
            builder.RegisterService(&service_);
          },
          &server_, &bound_port)) {
    LOG(ERROR) << "[video] 无法监听（" << kGrpcBindMaxAttempts
               << " 次尝试后仍无可用端口）";
    server_.reset();
    return false;
  }

  address_ = FormatHostPort(kLoopbackHost, bound_port);
  if (!registration_.RegisterBoundPort(bound_port)) {
    LOG(ERROR) << "[video] 名字服务注册失败 address=" << address_;
    server_.reset();
    return false;
  }

  LOG(INFO) << "[video] gRPC 已绑定 " << address_;
  return true;
}

void VideoServer::StopListeningLocked() {
  registration_.Deregister();
  server_.reset();
}

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
  registration_.Deregister();
  if (server_) {
    server_->Shutdown();
  }
}

bool VideoServer::Reload(const std::string &config_path) {
  if (!Config::Reload(config_path)) {
    return false;
  }
  LOG(INFO) << "[video] Reload 完成 bind=" << address_;
  return true;
}

}  // namespace recommendation
